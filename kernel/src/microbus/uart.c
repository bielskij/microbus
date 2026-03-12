// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/w1.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/list.h>

#include <linux/cdev.h>
#include <linux/device.h>

#include "common/protocol.h"
#include "common/protocol/packet/decoder.h"
#include "common/protocol/command.h"
#include "common/protocol/request.h"
#include "common/protocol/response.h"

#include "microbus/ioctl.h"

#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARN  2
#define DEBUG_LEVEL_LOG   3
#define DEBUG_LEVEL_DBG   4
#define DEBUG_LEVEL_TRACE 5

static int search_enable = 1;
module_param(search_enable, int, 0644);
MODULE_PARM_DESC(search_enable, "Enable automatic 1-Wire device search (1 - enabled, 0 - disabled, default: 1)");

static int debug = DEBUG_LEVEL_LOG;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (1=ERR, 2=WARN, 3=LOG, 4=DBG, 5=TRC)");

#define _REPORT(level, fmt, ...) do {\
    if (debug >= level) { \
        if (level >= DEBUG_LEVEL_DBG) { \
            pr_info(KBUILD_MODNAME " [%s:%d]: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); \
        } else { \
            pr_info(KBUILD_MODNAME ": " fmt "\n", ##__VA_ARGS__); \
        } \
    } \
} while (0);

#define _ERR(...)   _REPORT(DEBUG_LEVEL_ERROR, ##__VA_ARGS__)
#define _WARN(...)  _REPORT(DEBUG_LEVEL_WARN, ##__VA_ARGS__)
#define _LOG(...)   _REPORT(DEBUG_LEVEL_LOG, ##__VA_ARGS__)
#define _DBG(...)   _REPORT(DEBUG_LEVEL_DBG, ##__VA_ARGS__)
#define _TRACE(...) _REPORT(DEBUG_LEVEL_TRACE, ##__VA_ARGS__)

#define UBUS_ERR(x)   _ERR x
#define UBUS_WARN(x)  _WARN x
#define UBUS_LOG(x)   _LOG x
#define UBUS_DBG(x)   _DBG x
#define UBUS_TRACE(x) _TRACE x

#define UBUS_DUMP(title, buffer, bufferSize) do { \
    if (debug >= DEBUG_LEVEL_TRACE) { \
        pr_info(KBUILD_MODNAME " [%s:%d]: " title "\n", __func__, __LINE__); \
        for (size_t i = 0; i < bufferSize; i++) { \
            pr_cont("%02x, ", ((uint8_t*) buffer)[i]); \
            if (((i + 1) % 16) == 0) { \
                pr_cont("\n"); \
            } \
        } \
        pr_cont("\n"); \
    } \
} while (0);

#define DEFAULT_PACKET_SIZE 128
#define DEFAULT_RETRIES       4
#define DEFAULT_TIMEOUT_MS  500

typedef struct _UbusCmd {
    u8                id;
    struct completion workerCompletion;
    struct completion cmdCompletion;

    struct list_head list;

    void    *requestBuffer;
    size_t   requestBufferSize;
    ProtoPkt requestPacket;
    ProtoReq request;

    void    *responseBuffer;
    size_t   responseBufferSize;
    ProtoPkt responsePacket;
    ProtoRes response;

    int errorCode;
} UbusCmd;

typedef struct _UbusUart {
    struct tty_struct *tty;

    struct task_struct *worker;
    spinlock_t          workerQueueLock;
    struct list_head    workerQueue;

    wait_queue_head_t wq;

    atomic_t nextCmdId;

    ProtoPktDec packetDecoder;
    size_t      packetSize;

    UbusCmd *pendingCmd;
    bool     hwDetected;

    struct platform_device *platformDevice;

    struct i2c_adapter    i2cAdapter;

    struct w1_bus_master  w1Master;
    char                  w1MasterId[64];
    dev_t                 w1MasterCharDev;
    struct cdev           w1MasterCharCdev;
    struct class         *w1MasterClass;

    struct spi_controller *spiController;
    struct spi_device     *spiDevice;
} UbusUart;

static UbusCmd *cmd_init(UbusUart *ubus, UbusCmd *cmd, uint8_t cmdCode) {
    INIT_LIST_HEAD(&cmd->list);

    init_completion(&cmd->workerCompletion);
    init_completion(&cmd->cmdCompletion);

    cmd->id = (u8) atomic_inc_return(&ubus->nextCmdId);

    // Initialize request
    {
        proto_pkt_init(&cmd->requestPacket,  cmd->requestBuffer,  cmd->requestBufferSize, cmdCode, cmd->id);
        proto_pkt_init(&cmd->responsePacket, cmd->responseBuffer, cmd->responseBufferSize, 0, 0);

        proto_req_init(&cmd->request, cmd->requestPacket.payload, cmd->requestPacket.payloadSize, cmd->requestPacket.code);
    }

    return cmd;
}

static UbusCmd *cmd_alloc(UbusUart *ubus, uint8_t cmd) {
    UbusCmd *ret = kzalloc(sizeof(*ret), GFP_KERNEL);

    if (ret) {
        UBUS_DBG(("Allocating request and response buffer of size %zdB", ubus->packetSize));

        ret->requestBufferSize = ubus->packetSize;
        ret->requestBuffer     = kmalloc(ret->requestBufferSize, GFP_KERNEL);

        ret->responseBufferSize = ubus->packetSize;
        ret->responseBuffer     = kmalloc(ret->responseBufferSize, GFP_KERNEL);

        ret = cmd_init(ubus, ret, cmd);
    }

    return ret;
}

static int cmd_prepare(UbusUart *ubus, UbusCmd *cmd) {
    int ret = 0;

    // Request
    {
        cmd->errorCode = 0;

        if (cmd->requestBuffer == NULL) {
            ret = -ENOMEM;

        } else {
            proto_req_assign(&cmd->request, cmd->requestPacket.payload, cmd->requestPacket.payloadSize);
        }
    }

    return ret;
}

static int cmd_enqueue(UbusUart *ubus, UbusCmd *cmd) {
    int ret = 0;

    {
        unsigned long flags;

        cmd->requestPacket.payloadUsed = proto_req_encode(&cmd->request, cmd->requestPacket.payload, cmd->requestPacket.payloadSize);

        spin_lock_irqsave(&ubus->workerQueueLock, flags);
        {
            list_add_tail(&cmd->list, &ubus->workerQueue);
        }
        spin_unlock_irqrestore(&ubus->workerQueueLock, flags);

        wake_up(&ubus->wq);
    }

    return ret;
}

static void cmd_free(UbusCmd **cmd) {
    UbusCmd *c = cmd ? *cmd : NULL;

    if (c) {
        if (c->requestBuffer) {
            kfree(c->requestBuffer);
            c->requestBuffer = NULL;
        }

        if (c->responseBuffer) {
            kfree(c->responseBuffer);
            c->responseBuffer = NULL;
        }

        kfree(c);

        *cmd = NULL;
    }
}

static int cmd_wait(UbusCmd *cmd) {
    wait_for_completion(&cmd->cmdCompletion);

    return 0;
}

static void _w1SearchImpl(void *devData, struct w1_master *master, u8 searchType, w1_slave_found_callback callback) {
    UbusUart *ubus = (UbusUart *) devData;

    UBUS_TRACE(("[W1]: Search type: %02x", searchType));

    {
        UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_OW_TRANSFER);

        ProtoReqOwTransfer *req = &cmd->request.request.owTransfer;
        ProtoResOwTransfer *res = &cmd->response.response.owTransfer;

        int slaveCount = 0;

        req->type             = PROTO_OW_TRANSFER_TYPE_SEARCH_START;
        req->data.search.type = searchType;

        bool wasLast = false;
        do {
            cmd_prepare(ubus, cmd);
            cmd_enqueue(ubus, cmd);
            cmd_wait(cmd);

            UBUS_DBG(("[W1] Search step, status: %02x, rn: 0x%llx, descBit: %u, lastZero: %u, slaveCount: %d",
                res->status, res->data.search.romId, res->data.search.descBit, res->data.search.lastZero, slaveCount
            ));

            if (res->status != PROTO_OW_STATUS_SEARCH_STEP) {
                wasLast = true;
            }

            if (
                (res->status == PROTO_OW_STATUS_SEARCH_DONE_FOUND) ||
                (res->status == PROTO_OW_STATUS_SEARCH_STEP)
            ) {
                UBUS_DBG(("[W1] Calling callback with ID: 0x%llx", res->data.search.romId));

                callback(master, res->data.search.romId);

                slaveCount++;
            }

            if (! wasLast) {
                cmd = cmd_init(ubus, cmd, PROTO_CMD_OW_TRANSFER);

                req->type = PROTO_OW_TRANSFER_TYPE_SEARCH_STEP;

                req->data.search.type     = searchType;
                req->data.search.descBit  = res->data.search.descBit;
                req->data.search.lastZero = res->data.search.lastZero;
                req->data.search.romId    = res->data.search.romId;

                if (slaveCount == master->max_slave_count && (W1_WARN_MAX_COUNT & master->flags) == 0) {
                    /* Only max_slave_count will be scanned in a search,
                    * but it will start where it left off next search
                    * until all ids are identified and then it will start
                    * over.  A continued search will report the previous
                    * last id as the first id (provided it is still on the
                    * bus).
                    */
                    dev_info(&master->dev, "%s: max_slave_count %d reached, will continue next search.\n",
                        __func__, master->max_slave_count
                    );

                    set_bit(W1_WARN_MAX_COUNT, &master->flags);
                }
            }

        } while (! wasLast && (slaveCount < master->max_slave_count));
    }
}

static void _w1Search(void *devData, struct w1_master *master, u8 searchType, w1_slave_found_callback callback) {
    if (search_enable) {
        _w1SearchImpl(devData, master, searchType, callback);
    }
}

static u8 _w1ResetBus(void *devData) {
    u8 ret = 1;

    {
        UbusUart *ubus = (UbusUart *) devData;

        UBUS_TRACE(("[W1]: reseting bus"));

        {
            UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_OW_TRANSFER);
            if (cmd) {
                ProtoReqOwTransfer *t = &cmd->request.request.owTransfer;

                t->type = PROTO_OW_TRANSFER_TYPE_RESET;

                cmd_prepare(ubus, cmd);
                cmd_enqueue(ubus, cmd);
                cmd_wait(cmd);

                {
                    int errorCode = cmd->errorCode;

                    if (errorCode == 0) {
                        ProtoResOwTransfer *res = &cmd->response.response.owTransfer;

                        if (res->status == PROTO_OW_STATUS_OK) {
                            ret = 0;

                        } else if (res->status == PROTO_OW_STATUS_NO_PRESENCE) {
                            ret = 1;

                        } else {
                            UBUS_ERR(("Received unexpected status code %02x", res->status));

                            ret = -1;
                        }

                    } else {
                        ret = -1;
                    }
                }

                cmd_free(&cmd);
            }
        }
    }

    //  return -1=Error, 0=Device present, 1=No device present
    return ret;
}

static void _w1WriteBlock(void *devData, const u8 *buffer, int bufferLength) {
    UbusUart *ubus = (UbusUart *) devData;

    UBUS_TRACE(("[W1]: Writing block of length %d", bufferLength));

    if (bufferLength > 0) {
        UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_OW_TRANSFER);
        if (cmd != NULL) {
            int totalWritten = 0;

            do {
                ProtoReqOwTransfer *req = &cmd->request.request.owTransfer;

                req->type = PROTO_OW_TRANSFER_TYPE_WRITE;

                cmd_prepare(ubus, cmd);

                {
                    size_t toSendDataSize = min(bufferLength - totalWritten, req->data.transfer.dataSize);

                    if (toSendDataSize == 0) {
                        UBUS_WARN(("Read buffer too small - aborting"));

                        break;

                    } else {
                        req->data.transfer.dataSize = toSendDataSize;

                        if (toSendDataSize) {
                            memcpy(req->data.transfer.data, buffer + totalWritten, toSendDataSize);
                        }

                        cmd_enqueue(ubus, cmd);
                        cmd_wait(cmd);

                        if (cmd->errorCode == 0) {
                            totalWritten += toSendDataSize;

                        } else {
                            break;
                        }
                    }
                }

                cmd_init(ubus, cmd, PROTO_CMD_OW_TRANSFER);
            } while (totalWritten != bufferLength);

            cmd_free(&cmd);
        }
    }
}

static u8 _w1ReadBlock(void *devData, u8 *buffer, int bufferLength) {
    u8 totalRead = 0;

    UBUS_TRACE(("[W1]: Reading block of length %d", bufferLength));

    if (bufferLength > 0) {
        UbusUart *ubus = (UbusUart *) devData;

        UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_OW_TRANSFER);
        if (cmd != NULL) {
            do {
                ProtoReqOwTransfer *req = &cmd->request.request.owTransfer;
                ProtoResOwTransfer *res = &cmd->response.response.owTransfer;

                req->type = PROTO_OW_TRANSFER_TYPE_READ;

                cmd_prepare(ubus, cmd);

                {
                    size_t toReadDataSize = min(bufferLength - totalRead, req->data.transfer.dataSize);

                    if (toReadDataSize == 0) {
                        UBUS_WARN(("Read buffer too small - aborting"));

                        break;

                    } else {
                        req->data.transfer.dataSize = toReadDataSize;

                        cmd_enqueue(ubus, cmd);
                        cmd_wait(cmd);

                        if (cmd->errorCode == 0) {
                            uint16_t readSize = res->data.transfer.dataSize;

                            if (readSize == 0) {
                                UBUS_WARN(("Received 0 bytes instead of expected %u - interrupting", req->data.transfer.dataSize));

                                break;
                            }

                            memcpy(buffer + totalRead, res->data.transfer.data, readSize);

                            totalRead += readSize;

                        } else {
                            break;
                        }
                    }
                }

                cmd_init(ubus, cmd, PROTO_CMD_OW_TRANSFER);
            } while (totalRead != bufferLength);

            cmd_free(&cmd);
        }
    }

    return totalRead;
}

static u8 _w1ReadByte(void *devData) {
    u8 byte = 0;

    UBUS_TRACE(("[W1]: Reading single byte"));

    _w1ReadBlock(devData, &byte, 1);

    return byte;
}

static void _w1WriteByte(void *devData, u8 byte) {
    UBUS_TRACE(("[W1]: Writing single byte %02x", byte));

    _w1WriteBlock(devData, &byte, 1);
}

static u8 _w1TouchBit(void *devData, u8 bit) {
    u8 ret = 0;

    {
        UbusUart *ubus = (UbusUart *) devData;

        UBUS_TRACE(("[W1]: touching bit %u", bit));

        {
            UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_OW_TRANSFER);
            if (cmd) {
                ProtoReqOwTransfer *t = &cmd->request.request.owTransfer;

                t->type = PROTO_OW_TRANSFER_TYPE_TOUCH_BIT;

                t->data.touchBit.value = bit;

                cmd_prepare(ubus, cmd);
                cmd_enqueue(ubus, cmd);
                cmd_wait(cmd);

                {
                    int errorCode = cmd->errorCode;

                    if (errorCode == 0) {
                        ProtoResOwTransfer *res = &cmd->response.response.owTransfer;

                        if (res->status == PROTO_OW_STATUS_OK) {
                            ret = t->data.touchBit.value;
                        }
                    }
                }

                cmd_free(&cmd);
            }
        }
    }

    return ret;
}

static int _i2cXfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num) {
    int ret = 0;

    {
        UbusUart *ubus = (UbusUart *) adap->algo_data;

        UBUS_DBG(("[I2C] Requested transfer of %d I2C messages", num));

        for (int msgIndex = 0; msgIndex < num; msgIndex++) {
            struct i2c_msg *msg = &msgs[msgIndex];

            UBUS_DBG(("[I2C] Transfering message %d of %d, slave: %x, data length: %u, flags: %02x", msgIndex + 1, num, msg->addr, msg->len, msg->flags));

            size_t written = 0;

            do {
                UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_I2C_TRANSFER);
                if (cmd) {
                    ProtoReqI2CTransfer *tx = &cmd->request.request.i2cTransfer;

                    size_t toSendDataSize = min(msg->len - written, tx->dataSize);

                    tx->slaveAddress = msg->addr;

                    // Flags
                    {
                        tx->flags = 0;

                        if (msg->flags & I2C_M_RD) {
                            tx->flags |= PROTO_I2C_TRANSFER_FLAG_READ;
                        }

                        // Start condition
                        if (written == 0) {
                            if (msgIndex > 0) {
                                if (msgs[msgIndex - 1].flags & I2C_M_STOP) {
                                    // Force start after stop
                                    tx->flags |= PROTO_I2C_TRANSFER_FLAG_START;

                                // TODO: Verify
                                } else if ((msg->flags & I2C_M_NOSTART) == 0) {
                                    tx->flags |= PROTO_I2C_TRANSFER_FLAG_REPEATED_START;
                                }

                            } else {
                                tx->flags |= PROTO_I2C_TRANSFER_FLAG_START;
                            }
                        }

                        // Stop condition
                        if (written + toSendDataSize == msg->len) {
                            if (msg->flags & I2C_M_STOP) {
                                tx->flags |= PROTO_I2C_TRANSFER_FLAG_STOP;

                            } else if (msgIndex == num - 1) {
                                tx->flags |= PROTO_I2C_TRANSFER_FLAG_STOP;
                            }

                        } else {
                            tx->flags |= PROTO_I2C_TRANSFER_FLAG_CONT;
                        }
                    }

                    tx->dataSize = toSendDataSize;

                    cmd_prepare(ubus, cmd);

                    if (! (msg->flags & I2C_M_RD)) {
                        if (toSendDataSize) {
                            memcpy(tx->data, msg->buf + written, toSendDataSize);
                        }
                    }

                    UBUS_DBG(("[I2C] Scheduling transfer command START/REP: %d/%d, STOP: %d, READ: %d, CONT: %d, len: %u",
                        (tx->flags & PROTO_I2C_TRANSFER_FLAG_START) != 0,
                        (tx->flags & PROTO_I2C_TRANSFER_FLAG_REPEATED_START) != 0,
                        (tx->flags & PROTO_I2C_TRANSFER_FLAG_STOP) != 0,
                        (tx->flags & PROTO_I2C_TRANSFER_FLAG_READ) != 0,
                        (tx->flags & PROTO_I2C_TRANSFER_FLAG_CONT) != 0,
                        tx->dataSize
                    ));

                    cmd_enqueue(ubus, cmd);
                    cmd_wait(cmd);

                    {
                        int errorCode = cmd->errorCode;

                        if (errorCode == 0) {
                            ProtoResI2cTransfer *t = &cmd->response.response.i2cTransfer;

                            UBUS_DBG(("[I2C] Have transfer command response, status: %u, cmd: %u", t->status, cmd->response.cmd));

                            switch (t->status) {
                                case PROTO_I2C_STATUS_OK:
                                    if (msg->flags & I2C_M_RD) {
                                        memcpy(msg->buf + written, t->rxBuffer, t->rxBufferSize);
                                    }
                                    break;

                                case PROTO_I2C_STATUS_NAK_DATA:
                                    errorCode = -EIO;
                                    break;

                                case PROTO_I2C_STATUS_NAK_ADDRESS:
                                    errorCode = -ENXIO;
                                    break;

                                case PROTO_I2C_STATUS_ARB_LOST:
                                    errorCode = -EIO;
                                    break;

                                default:
                                    errorCode = -EINVAL;
                            }
                        }

                        cmd_free(&cmd);

                        if (errorCode == 0) {
                            written += toSendDataSize;

                        } else {
                            ret = errorCode;
                            break;
                        }
                    }
                }
            } while (written != msg->len);

            if (ret < 0) {
                break;
            }

            ++ret;
        }
    }

    UBUS_DBG(("[I2C] Transfer of %d I2C messages has finished with status: %d", num, ret));

    return ret;
}

static u32 _i2cFunctionality(struct i2c_adapter *adap) {
    return I2C_FUNC_I2C | I2C_FUNC_NOSTART | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm _ubusI2cAlgo = {
    .master_xfer   = _i2cXfer,
    .functionality = _i2cFunctionality,
};

static int _w1DevOpen(struct inode *inode, struct file *file) {
    UBUS_DBG(("CALL"));

    file->private_data = container_of(inode->i_cdev, UbusUart, w1MasterCharCdev);

    return 0;
}

static int _w1DevRelease(struct inode *inode, struct file *file) {
    UBUS_DBG(("CALL"));

    file->private_data = NULL;

    return 0;
}

static long _w1DevIoctl(struct file *file, unsigned int ioctlCmd, unsigned long arg) {
    UbusUart *ubus = (UbusUart *) file->private_data;

    UBUS_DBG(("CALL"));

    if (_IOC_TYPE(ioctlCmd) != MICROBUS_IOC_MAGIC) {
        return -ENOTTY;
    }

    switch (ioctlCmd) {
        case MICROBUS_IOC_RESET:
            {
                __u8 presence;

                presence = _w1ResetBus(ubus);

                if (copy_to_user((__u8 __user *) arg, &presence, sizeof(presence))) {
                    return -EFAULT;
                }
            }
            break;

        case MICROBUS_IOC_SEARCH_START:
        case MICROBUS_IOC_SEARCH_STEP:
            {
                struct MicrobusSearchStep step;

                if (copy_from_user(&step, (void __user *) arg, sizeof(step))) {
                    return -EFAULT;
                }

                {
                    UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_OW_TRANSFER);

                    ProtoReqOwTransfer *req = &cmd->request.request.owTransfer;
                    ProtoResOwTransfer *res = &cmd->response.response.owTransfer;

                    if (ioctlCmd == MICROBUS_IOC_SEARCH_START) {
                        req->type = PROTO_OW_TRANSFER_TYPE_SEARCH_START;

                    } else {
                        req->type = PROTO_OW_TRANSFER_TYPE_SEARCH_STEP;

                        req->data.search.descBit  = step.descBit;
                        req->data.search.lastZero = step.lastZero;
                        req->data.search.romId    = step.rn;
                    }

                    req->data.search.type = step.type;

                    cmd_prepare(ubus, cmd);
                    cmd_enqueue(ubus, cmd);
                    cmd_wait(cmd);

                    if (res->status != PROTO_OW_STATUS_SEARCH_STEP) {
                        step.wasLast = true;

                    } else {
                        step.wasLast = false;
                    }

                    if (
                        (res->status == PROTO_OW_STATUS_SEARCH_DONE_FOUND) ||
                        (res->status == PROTO_OW_STATUS_SEARCH_STEP)
                    ) {
                        step.found = true;

                    } else {
                        step.found = false;
                    }

                    step.descBit  = res->data.search.descBit;
                    step.lastZero = res->data.search.lastZero;
                    step.rn       = res->data.search.romId;

                    cmd_free(&cmd);
                }

                if (copy_to_user((void __user *) arg, &step, sizeof(step))) {
                    return -EFAULT;
                }
            }
            break;
    }

    return 0;
}

static char *_w1DevNode(const struct device *dev, umode_t *mode) {
    if (mode) {
        *mode = 0666;
    }

    return NULL;
}

static ssize_t _w1DevRead(struct file *file, char __user *data, size_t size, loff_t *ppos) {
    ssize_t ret = size;

    {
        UbusUart *ubus = (UbusUart *) file->private_data;

        UBUS_DBG(("CALL buffer %p, size: %zd", data, size));

        *ppos = 0;

        if (size > 0) {
            u8 *kernelBuffer = kmalloc(size, GFP_KERNEL);
            if (kernelBuffer != NULL) {
                _w1ReadBlock(ubus, kernelBuffer, size);

                if (copy_to_user(data, kernelBuffer, size)) {
                    ret = -EFAULT;
                }

                kfree(kernelBuffer);
            }
        }
    }

    return ret;
}

static ssize_t _w1DevWrite(struct file *file, const char __user *data, size_t size, loff_t *ppos) {
    ssize_t ret = size;

    {
        UbusUart *ubus = (UbusUart *) file->private_data;

        UBUS_DBG(("CALL buffer %p, size: %zd", data, size));

        *ppos = 0;

        if (size > 0) {
            u8 *kernelBuffer = kmalloc(size, GFP_KERNEL);
            if (kernelBuffer != NULL) {
                if (copy_from_user(kernelBuffer, data, size)) {
                    ret = -EFAULT;

                } else {
                    _w1WriteBlock(ubus, kernelBuffer, size);
                }

                kfree(kernelBuffer);
            }
        }
    }

    return ret;
}

static const struct file_operations _ubusW1Fops = {
    .owner = THIS_MODULE,

    .open           = _w1DevOpen,
    .unlocked_ioctl = _w1DevIoctl,
    .read           = _w1DevRead,
    .write          = _w1DevWrite,
    .release        = _w1DevRelease,
    .llseek         = noop_llseek
};

static struct spi_board_info _spiChip = {
	.modalias = "microbus-spi",
};

static int _spiTransferOne(struct spi_controller *ctlr, struct spi_device *spi, struct spi_transfer *transfer) {
    UBUS_DBG(("CALL buffer tx: %p, size: %d, rx: %p, size: %d", transfer->tx_buf, transfer->len, transfer->rx_buf, transfer->len));

    return 0;
}

static size_t _spiMaxTransferSize(struct spi_device *spi) {
    UbusUart *ubus = (UbusUart *) spi_controller_get_devdata(spi->controller);

    {
        ProtoReq req;

        proto_req_init(&req, NULL, ubus->packetSize, PROTO_CMD_SPI_TRANSFER);

        UBUS_DBG(("CALL, returning %u", req.request.spiTransfer.txBufferSize));

        return req.request.spiTransfer.txBufferSize;
    }
}

static void _sendPacket(UbusUart *ubus, ProtoPkt *pkt) {
    // TODO: Handle errors!
    ubus->tty->ops->write(ubus->tty, pkt->header,  pkt->headerUsed);
    ubus->tty->ops->write(ubus->tty, pkt->payload, pkt->payloadUsed);
    ubus->tty->ops->write(ubus->tty, pkt->footer,  pkt->footerUsed);
}

static void _handleCmd(UbusUart *ubus, UbusCmd *cmd, bool force) {
    if (ubus->hwDetected || force) {
        unsigned long flags;
        int maxTries = DEFAULT_RETRIES + 1;
        int i;

        spin_lock_irqsave(&ubus->workerQueueLock, flags);
        {
            ubus->pendingCmd = cmd;

            proto_pkt_dec_reset(&ubus->packetDecoder, &cmd->responsePacket);
        }
        spin_unlock_irqrestore(&ubus->workerQueueLock, flags);

        proto_pkt_encode(&cmd->requestPacket);

        for (i = 0; i < maxTries; i++) {
            _sendPacket(ubus, &cmd->requestPacket);

            long waitRet = wait_for_completion_interruptible_timeout(&cmd->workerCompletion, msecs_to_jiffies(DEFAULT_TIMEOUT_MS));
            if (waitRet > 0) {
                if (cmd->errorCode == 0) {
                    break;
                }

            } else if (waitRet < 0) {
                i = waitRet;
                break;
            }
        }

        spin_lock_irqsave(&ubus->workerQueueLock, flags);
        {
            ubus->pendingCmd = NULL;

            if (i < 0) {
                cmd->errorCode = i;

            } else if ((i == maxTries) && (cmd->errorCode == 0)) {
                cmd->errorCode = -ETIMEDOUT;
            }
        }
        spin_unlock_irqrestore(&ubus->workerQueueLock, flags);

    } else {
        cmd->errorCode = -ENODEV;
    }

    complete(&cmd->cmdCompletion);
}

static int _workerRoutine(void *arg) {
    UbusUart *ubus = (UbusUart *) arg;

    UBUS_DBG(("Microbus worker has started, probing device"));

    ubus->hwDetected = false;

    {
        UbusCmd *cmd = cmd_alloc(ubus, PROTO_CMD_GET_INFO);
        if (cmd) {
            int ret = cmd_prepare(ubus, cmd);
            if (ret == 0) {
                _handleCmd(ubus, cmd, true);

                if (cmd->errorCode == 0) {
                    ProtoRes *response = &cmd->response;

                    ProtoResGetInfo *info = &response->response.getInfo;

                    UBUS_DBG(("Received response cmd: %u, version %u.%u, payload size: %u, features: %02x",
                        response->cmd, info->version.major, info->version.minor, info->packetSize, info->features
                    ));

                    if (
                        info->version.major != PROTO_VERSION_MAJOR ||
                        info->version.minor != PROTO_VERSION_MINOR
                    ) {
                        UBUS_ERR(("Received response from device that uses not supported protocol version %u.%u != %u.%u",
                            info->version.major, info->version.minor, PROTO_VERSION_MAJOR, PROTO_VERSION_MINOR
                        ));

                    } else {
                        int ret;

                        ubus->hwDetected = true;

                        if (ubus->packetSize != info->packetSize) {
                            if (info->packetSize == 0) {
                                UBUS_ERR(("Received hardware information with packet size 0 - unexpected value, discarding the device"));

                                ubus->hwDetected = false;

                            } else {
                                ubus->packetSize = info->packetSize;
                            }
                        }

                        UBUS_LOG(("Detected hardware with protocol %u.%u, payload size: %u, features: i2c: %c, 1w: %c, spi: %c",
                            info->version.major, info->version.minor, info->packetSize,
                            (info->features & PROTO_FEATURE_I2C) != 0 ? 'Y' : 'N',
                            (info->features & PROTO_FEATURE_OW) != 0 ? 'Y' : 'N',
                            (info->features & PROTO_FEATURE_SPI) != 0 ? 'Y' : 'N'
                        ));

                        if ((info->features & PROTO_FEATURE_I2C) != 0) {
                            UBUS_DBG(("Registering new i2c device in kernel"));

                            ret = i2c_add_adapter(&ubus->i2cAdapter);
                            if (ret == 0) {
                                UBUS_LOG(("Created new i2c device i2c-%d", ubus->i2cAdapter.nr));
                            }
                        }

                        if ((info->features & PROTO_FEATURE_OW) != 0) {
                            UBUS_DBG(("Reginstering new 1wire device in kernel"));

                            ubus->w1Master.data = ubus;

                            ret = w1_add_master_device(&ubus->w1Master);
                            if (ret == 0) {
                                UBUS_LOG(("Created new 1wire device"));

                            } else {
                                ubus->w1Master.data = NULL;
                            }

                            if (ret == 0) {
                                ret = alloc_chrdev_region(&ubus->w1MasterCharDev, 0, 1, "microbus_ow");
                                if (ret != 0) {
                                    UBUS_ERR(("Can't get major number for new w1 device"));

                                } else {
                                    cdev_init(&ubus->w1MasterCharCdev, &_ubusW1Fops);

                                    ubus->w1MasterCharCdev.owner = THIS_MODULE;

                                    ret = cdev_add(&ubus->w1MasterCharCdev, ubus->w1MasterCharDev, 1);
                                    if (ret != 0) {
                                        UBUS_ERR(("Can't add cdev device for w1"));

                                        unregister_chrdev_region(ubus->w1MasterCharDev, 1);

                                    } else {
                                        ubus->w1MasterClass = class_create("microbus_ow");

                                        // sets proper rights on device node
                                        ubus->w1MasterClass->devnode = _w1DevNode;

                                        if (IS_ERR(ubus->w1MasterClass)) {
                                            UBUS_ERR(("Failed to create w1 class"));

                                            cdev_del(&ubus->w1MasterCharCdev);
                                            unregister_chrdev_region(ubus->w1MasterCharDev, 1);

                                        } else {
                                            struct device *dev = device_create(ubus->w1MasterClass, NULL, ubus->w1MasterCharDev, ubus, "ow-%d", MINOR(ubus->w1MasterCharDev));
                                            if (IS_ERR(dev)) {
                                                UBUS_ERR(("Failed to create w1 device"));

                                                class_destroy(ubus->w1MasterClass);
                                                cdev_del(&ubus->w1MasterCharCdev);
                                                unregister_chrdev_region(ubus->w1MasterCharDev, 1);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if ((info->features & PROTO_FEATURE_SPI) != 0) {
                            struct spi_controller *spi = ubus->spiController;

                            if (spi) {
                                spi->mode_bits &= ~SPI_MODE_X_MASK;

                                if (info->spiMode0) spi->mode_bits |= SPI_MODE_0;
                                if (info->spiMode1) spi->mode_bits |= SPI_MODE_1;
                                if (info->spiMode2) spi->mode_bits |= SPI_MODE_2;
                                if (info->spiMode3) spi->mode_bits |= SPI_MODE_3;

                                ret = spi_register_controller(spi);
                                if (ret != 0) {
                                    UBUS_ERR(("Failed to register SPI controller"));

                                } else {
                                    UBUS_LOG(("Registered SPI controller spi-%d", spi->bus_num));

                                    _spiChip.mode    = spi->mode_bits;
                                    _spiChip.bus_num = spi->bus_num;

                                    ubus->spiDevice = spi_new_device(spi, &_spiChip);
                                    if (! ubus->spiDevice) {
                                        UBUS_ERR(("Failed to register SPI device"));
                                    }
                                }
                            } else {
                                UBUS_WARN(("SPI is null!"));
                            }
                        }
                    }

                } else {
                    UBUS_WARN(("Probe step of device '%s' has failed with error %d", ubus->tty->name, cmd->errorCode));

                    ubus->hwDetected = false;
                }
            }

            cmd_free(&cmd);
        }
    }

    while (! kthread_should_stop()) {
        UbusCmd *cmd = NULL;

        UBUS_DBG(("Microbus worker is waiting for new events"));

        wait_event_interruptible(ubus->wq, ! list_empty(&ubus->workerQueue) || kthread_should_stop());
        if (kthread_should_stop()) {
            break;
        }

        UBUS_DBG(("Microbus worker has been woken up"));

        {
            unsigned long flags;

            spin_lock_irqsave(&ubus->workerQueueLock, flags);
            {
                if (list_empty(&ubus->workerQueue)) {
                    spin_unlock_irqrestore(&ubus->workerQueueLock, flags);
                    {
                        wait_event_interruptible(ubus->wq, ! list_empty(&ubus->workerQueue) || kthread_should_stop());
                    }
                    spin_lock_irqsave(&ubus->workerQueueLock, flags);
                }

                if (! list_empty(&ubus->workerQueue)) {
                    cmd = list_first_entry(&ubus->workerQueue, UbusCmd, list);

                    list_del(&cmd->list);
                }
            }

            spin_unlock_irqrestore(&ubus->workerQueueLock, flags);
        }

        if (! cmd) {
            continue;
        }

        if (! kthread_should_stop()) {
            _handleCmd(ubus, cmd, false);
        }
    }

    UBUS_DBG(("Microbus worker exiting"));

    return 0;
}

static size_t _ldiscReceive2(struct tty_struct *tty, const u8 *cp, const u8 *fp, size_t count) {
    size_t ret = 0;

    UBUS_DUMP("_ldiscReceive2: ", cp, count);

    {
        UbusUart *ubus = (UbusUart *) tty->disc_data;

        unsigned long flags;

        spin_lock_irqsave(&ubus->workerQueueLock, flags);
        {
            UbusCmd *cmd = ubus->pendingCmd;
            if (! cmd) {
                UBUS_ERR(("Received %zd bytes but there is no pending command - discarding %zd bytes", count, ret));

            } else {
                while (ret < count) {
                    ProtoPkt *pkt = &cmd->responsePacket;

                    uint8_t decRet = proto_pkt_dec_putByte(&ubus->packetDecoder, cp[ret++], pkt);
                    if (decRet != PROTO_PKT_DES_RET_IDLE) {
                        uint8_t errorCode = PROTO_PKT_DES_RET_GET_ERROR_CODE(decRet);

                        if (errorCode != PROTO_NO_ERROR) {
                            UBUS_ERR(("Received protocol error: %u for command id: %u",
                                errorCode, cmd->requestPacket.id
                            ));

                            cmd->errorCode = -EINVAL;

                        } else {
                            bool receivedExpectedResponse = false;

                            if (pkt->id != cmd->requestPacket.id) {
                                UBUS_WARN(("Received successful response for different ID (id %u != %u)", pkt->id, cmd->requestPacket.id));

                            } else {
                                UBUS_DBG(("Received successful response for command %u, id: %u, payloadLength: %u/%u (read %zd of %zd)",
                                    pkt->code, pkt->id, pkt->payloadUsed, pkt->payloadSize, ret, count
                                ));

                                // Decode response
                                proto_res_init(&cmd->response, pkt->payload, pkt->payloadUsed, cmd->requestPacket.code);

                                if (! proto_res_decode(&cmd->response, pkt->payload, pkt->payloadUsed)) {
                                    UBUS_WARN(("Can't decode incoming command!"));

                                } else {
                                    proto_res_assign(&cmd->response, pkt->payload, pkt->payloadUsed);

                                    receivedExpectedResponse = true;
                                }
                            }

                            if (receivedExpectedResponse) {
                                complete(&cmd->workerCompletion);
                            }
                        }

                        proto_pkt_dec_reset(&ubus->packetDecoder, NULL);
                    }
                }
            }
        }
        spin_unlock_irqrestore(&ubus->workerQueueLock, flags);
    }

    return ret;
}

static void _ldiscCleanup(UbusUart **ubus) {
    UbusUart *b = ubus != NULL ? *ubus : NULL;

    if (b == NULL) {
        UBUS_WARN(("Called on NULL ubus context - skipping"));
        return;
    }

    i2c_del_adapter(&b->i2cAdapter);

    if (b->w1Master.data != NULL) {
        w1_remove_master_device(&b->w1Master);

        device_destroy(b->w1MasterClass, b->w1MasterCharDev);
        class_destroy(b->w1MasterClass);
        cdev_del(&b->w1MasterCharCdev);
        unregister_chrdev_region(b->w1MasterCharDev, 1);
    }

    if (b->spiDevice) {
        spi_unregister_device(b->spiDevice);
    }

    if (b->spiController) {
        spi_unregister_controller(b->spiController);
    }

    if (b->platformDevice) {
        platform_device_unregister(b->platformDevice);
    }

    if (b->worker) {
        UBUS_DBG(("Stopping ubus worker"));

        kthread_stop(b->worker);
    }

    UBUS_DBG(("Releasing ubus context"));

    kfree(b);

    *ubus = NULL;
}

static int _ldiscOpen(struct tty_struct *tty) {
    int ret = 0;

    {
        UbusUart *ubus = NULL;

        UBUS_LOG(("Opening Microbus line discipline for '%s'", tty->name));

        if (! tty->ops->write) {
            ret = -EOPNOTSUPP;
        }

        if (ret == 0) {
            ubus = kmalloc(sizeof(*ubus), GFP_KERNEL);
            if (ubus == NULL) {
                ret = -ENOMEM;
            }
        }

        if (ret == 0) {
            ubus->packetSize = DEFAULT_PACKET_SIZE;
            ubus->pendingCmd = NULL;

            INIT_LIST_HEAD(&ubus->workerQueue);
            spin_lock_init(&ubus->workerQueueLock);
            init_waitqueue_head(&ubus->wq);

            atomic_set(&ubus->nextCmdId, 0);

            tty_ldisc_flush(tty);

            tty->disc_data = ubus;

            ubus->tty = tty;
        }

        if (ret == 0) {
            ubus->platformDevice = platform_device_register_simple("microbus-bridge", -1, NULL, 0);
            if (! ubus->platformDevice) {
                UBUS_WARN(("Failed to create microbus platform device"));
            }
        }

        if (ret == 0) {
            struct i2c_adapter *i2c = &ubus->i2cAdapter;

            i2c->owner     = THIS_MODULE;
            i2c->class     = I2C_CLASS_HWMON;
            i2c->retries   = 0;

            i2c->algo_data = ubus;
            i2c->algo      = &_ubusI2cAlgo;

            strncpy(i2c->name, "microbus-i2c", sizeof(i2c->name));
        }

        if (ret == 0) {
            struct w1_bus_master *w1 = &ubus->w1Master;

            strncpy(ubus->w1MasterId, "microbus-ow", sizeof(ubus->w1MasterId) - 1);

            w1->read_byte   = _w1ReadByte;
            w1->write_byte  = _w1WriteByte;

            w1->read_block  = _w1ReadBlock;
            w1->write_block = _w1WriteBlock;

            w1->touch_bit   = _w1TouchBit;

            w1->reset_bus   = _w1ResetBus;
            w1->search      = _w1Search;

            w1->dev_id = ubus->w1MasterId;
            w1->data   = NULL;
        }

        if (ret == 0) {
            if (ubus->platformDevice) {
                struct spi_controller *spi = spi_alloc_host(&ubus->platformDevice->dev, 0);

                if (spi) {
                    spi->auto_runtime_pm    = false;
                    spi->bits_per_word_mask = SPI_BPW_MASK(8);
                    spi->bus_num            = -1;

                    spi->transfer_one       = _spiTransferOne;
                    spi->max_transfer_size  = _spiMaxTransferSize;

                    spi_controller_set_devdata(spi, ubus);

                    ubus->spiController = spi;

                } else {
                    UBUS_WARN(("Failed to allocate SPI host controller"));
                }
            }
        }

        if (ret == 0) {
            ubus->worker = kthread_run(_workerRoutine, ubus, "ubus_uart_worker");
            if (IS_ERR(ubus->worker)) {
                ret = PTR_ERR(ubus->worker);
            }
        }

        if (ret != 0) {
            _ldiscCleanup(&ubus);
        }
    }

    return ret;
}

static void _ldiscClose(struct tty_struct *tty) {
    UBUS_LOG(("Closing Microbus line discipline used by '%s'", tty->name));

    _ldiscCleanup((UbusUart **) &tty->disc_data);
}

static struct tty_ldisc_ops _ldiscOps = {
    .owner = THIS_MODULE,
    .name  = "Microbus over UART line discipline",
    .num   = N_DEVELOPMENT,

    .open         = _ldiscOpen,
    .close        = _ldiscClose,
    .receive_buf2 = _ldiscReceive2
};

static int __init _ubus_uart_init(void) {
    int ret = tty_register_ldisc(&_ldiscOps);

    if (ret != 0) {
        UBUS_ERR(("Cannot register Microbus line discipline protocol"));
    }

    return ret;
}

static void __exit _ubus_uart_exit(void) {
    tty_unregister_ldisc(&_ldiscOps);
}

module_init(_ubus_uart_init);
module_exit(_ubus_uart_exit);

MODULE_AUTHOR("Jarosław Bielski <bielski.j@gmail.com>");
MODULE_DESCRIPTION("microbus over UART kernel driver");
MODULE_LICENSE("GPL");

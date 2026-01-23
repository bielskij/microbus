// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/tty.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/list.h>

#include "common/protocol.h"
#include "common/protocol/packet/decoder.h"
#include "common/protocol/command.h"
#include "common/protocol/request.h"
#include "common/protocol/response.h"

#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARN  2
#define DEBUG_LEVEL_LOG   3
#define DEBUG_LEVEL_DBG   4
#define DEBUG_LEVEL_TRACE 5

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

    struct i2c_adapter i2cAdapter;
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

static int _i2cXfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num) {
    int ret = 0;

    {
        UbusUart *ubus = (UbusUart *) adap->algo_data;

        UBUS_DBG(("Requested transfer of %d I2C messages", num));

        for (int msgIndex = 0; msgIndex < num; msgIndex++) {
            struct i2c_msg *msg = &msgs[msgIndex];

            UBUS_DBG(("Transfering message %d of %d, slave: %x, data length: %u, flags: %02x", msgIndex + 1, num, msg->addr, msg->len, msg->flags));

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

                    UBUS_DBG(("Scheduling transfer command START/REP: %d/%d, STOP: %d, READ: %d, CONT: %d, len: %u", 
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
                            
                            UBUS_DBG(("Have transfer command response, status: %u, cmd: %u", t->status, cmd->response.cmd));

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

    UBUS_DBG(("Transfer of %d I2C messages has finished with status: %d", num, ret));

    return ret;
}

static u32 _i2cFunctionality(struct i2c_adapter *adap) {
    return I2C_FUNC_I2C | I2C_FUNC_NOSTART | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm _ubusI2cAlgo = {
    .master_xfer   = _i2cXfer,
    .functionality = _i2cFunctionality,
};

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

                        UBUS_LOG(("Detected hardware with protocol %u.%u, payload size: %u, features: i2c: %c", 
                            info->version.major, info->version.minor, info->packetSize, 
                            (info->features & PROTO_FEATURE_I2C) != 0 ? 'Y' : 'N'
                        ));

                        if ((info->features & PROTO_FEATURE_I2C) != 0) {
                            UBUS_DBG(("Registering new i2c device in kernel"));
                            
                            ret = i2c_add_adapter(&ubus->i2cAdapter);
                            if (ret == 0) {
                                UBUS_LOG(("Created new i2c device i2c-%d", ubus->i2cAdapter.nr));
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
            ubus->i2cAdapter.owner     = THIS_MODULE;
            ubus->i2cAdapter.class     = I2C_CLASS_HWMON;
            ubus->i2cAdapter.retries   = 0;

            ubus->i2cAdapter.algo_data = ubus;
            ubus->i2cAdapter.algo      = &_ubusI2cAlgo;
    
            strncpy(ubus->i2cAdapter.name, "microbus-i2c", sizeof(ubus->i2cAdapter.name));
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
#include <linux/types.h>
#include <ipc/gpr-lite.h>
#include <elliptic/elliptic_data_io.h>
#include <linux/delay.h>
#include "audio_prm.h"

#define ELLIPTIC_SET_PARAMS_SIZE			114
#define ELLIPTIC_ULTRASOUND_MODULE_TX			0x0F010201
#define ELLIPTIC_ULTRASOUND_MODULE_RX			0x0FF10202
#define ULTRASOUND_OPCODE				0x0FF10204

/* This need to be updated for all platforms */
#define ELLIPTIC_PORT_ID				0x4005

/** Sequence of Elliptic Labs Ultrasound module parameters */
struct prm_ultrasound_set_params_t {
	uint32_t  payload[ELLIPTIC_SET_PARAMS_SIZE];
} __packed;

/** Sequence of Elliptic Labs Ultrasound module parameters */

/** Elliptic GPR public  */

int32_t ultrasound_gpr_set_parameter(int32_t port_id, uint32_t param_id,
	u8 *user_params, int32_t length);

int32_t elliptic_process_gpr_payload(uint32_t *payload);

int elliptic_notify_gain_change_msg(int component_id, int gaindb);

struct gpr_param_hdr {
    uint32_t module_id;
    uint16_t instance_id;
    uint16_t reserved;
    uint32_t param_id;
    uint32_t param_size;
} __packed;

struct prm_ultrasound_config_command {
	struct gpr_hdr                      hdr;
	struct apm_cmd_header_t             param;
        uint16_t                            pkt_size;
	struct apm_module_param_data_t      pdata;
	struct prm_ultrasound_set_params_t  prot_config;
} __packed;

typedef struct prm_ultrasound_state {
	atomic_t us_gpr_state;
	struct gpr_device *ptr_gpr;
	atomic_t *ptr_status;
	atomic_t *ptr_state;
	wait_queue_head_t *ptr_wait;
	struct mutex *ptr_prm_gpr_lock;
	int timeout_ms;
} prm_ultrasound_state_t;

extern prm_ultrasound_state_t elus_prm;

/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery module */

#ifndef __USB_PD_H
#define __USB_PD_H

#include "common.h"

enum pd_errors {
	PD_ERR_INVAL = -1,      /* Invalid packet */
	PD_ERR_HARD_RESET = -2, /* Got a Hard-Reset packet */
	PD_ERR_CRC = -3,        /* CRC mismatch */
	PD_ERR_ID = -4,         /* Invalid ID number */
};

/* incoming packet event (for the USB PD task) */
#define PD_EVENT_RX (1<<2)

/* --- PD data message helpers --- */
#define PDO_MAX_OBJECTS   7
#define PDO_MODES (PDO_MAX_OBJECTS - 1)

/* PDO : Power Data Object */
/*
 * 1. The vSafe5V Fixed Supply Object shall always be the first object.
 * 2. The remaining Fixed Supply Objects,
 *    if present, shall be sent in voltage order; lowest to highest.
 * 3. The Battery Supply Objects,
 *    if present shall be sent in Minimum Voltage order; lowest to highest.
 * 4. The Variable Supply (non battery) Objects,
 *    if present, shall be sent in Minimum Voltage order; lowest to highest.
 */
#define PDO_TYPE_FIXED    (0 << 30)
#define PDO_TYPE_BATTERY  (1 << 30)
#define PDO_TYPE_VARIABLE (2 << 30)
#define PDO_TYPE_MASK     (3 << 30)

#define PDO_FIXED_DUAL_ROLE (1 << 29) /* Dual role device */
#define PDO_FIXED_SUSPEND   (1 << 28) /* USB Suspend supported */
#define PDO_FIXED_EXTERNAL  (1 << 27) /* Externally powered */
#define PDO_FIXED_COMM_CAP  (1 << 26) /* USB Communications Capable */
#define PDO_FIXED_PEAK_CURR () /* [21..20] Peak current */
#define PDO_FIXED_VOLT(mv)  (((mv)/50) << 10) /* Voltage in 50mV units */
#define PDO_FIXED_CURR(ma)  (((ma)/10) << 0)  /* Max current in 10mA units */

#define PDO_FIXED(mv, ma, flags) (PDO_FIXED_VOLT(mv) |\
				  PDO_FIXED_CURR(ma) | (flags))

#define PDO_VAR_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_VAR_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 0)

#define PDO_VAR(min_mv, max_mv, op_ma) \
				(PDO_VAR_MIN_VOLT(min_mv) | \
				 PDO_VAR_MAX_VOLT(max_mv) | \
				 PDO_VAR_OP_CURR(op_ma))

#define PDO_BATT_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_BATT_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_BATT_OP_POWER(mw) ((((mw) / 10) & 0x3FF) << 0)

#define PDO_BATT(min_mv, max_mv, op_mw) \
				(PDO_BATT_MIN_VOLT(min_mv) | \
				 PDO_BATT_MAX_VOLT(max_mv) | \
				 PDO_BATT_OP_POWER(op_mw))

/* RDO : Request Data Object */
#define RDO_OBJ_POS(n)             (((n) & 0x7) << 28)
#define RDO_GIVE_BACK              (1 << 27)
#define RDO_CAP_MISMATCH           (1 << 26)
#define RDO_COMM_CAP               (1 << 25)
#define RDO_NO_SUSPEND             (1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) ((((ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      ((((mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     ((((mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags) \
				(RDO_OBJ_POS(n) | (flags) | \
				RDO_FIXED_VAR_OP_CURR(op_ma) | \
				RDO_FIXED_VAR_MAX_CURR(max_ma))


#define RDO_BATT(n, op_mw, max_mw, flags) \
				(RDO_OBJ_POS(n) | (flags) | \
				RDO_BATT_OP_POWER(op_mw) | \
				RDO_BATT_MAX_POWER(max_mw))

/* BDO : BIST Data Object */
#define BDO_MODE_RECV       (0 << 28)
#define BDO_MODE_TRANSMIT   (1 << 28)
#define BDO_MODE_COUNTERS   (2 << 28)
#define BDO_MODE_CARRIER0   (3 << 28)
#define BDO_MODE_CARRIER1   (4 << 28)
#define BDO_MODE_CARRIER2   (5 << 28)
#define BDO_MODE_CARRIER3   (6 << 28)
#define BDO_MODE_EYE        (7 << 28)

#define BDO(mode, cnt)      ((mode) | ((cnt) & 0xFFFF))

/* TODO(tbroch) is there a finite number for these in the spec */
#define SVID_DISCOVERY_MAX 16

/* function table for alternate mode capable responders */
struct svdm_response {
	int (*identity)(int port, uint32_t *payload);
	int (*svids)(int port, uint32_t *payload);
	int (*modes)(int port, uint32_t *payload);
	int (*enter_mode)(int port, uint32_t *payload);
	int (*exit_mode)(int port, uint32_t *payload);
};

/* defined in <board>/usb_pd_policy.c */
extern const struct svdm_response svdm_rsp;

/* Each SVID is allowed to have 6 modes */
enum dfp_amode {
	dfp_amode_none = 0,
	dfp_amode1,
	dfp_amode2,
	dfp_amode3,
	dfp_amode4,
	dfp_amode5,
	dfp_amode6,
};

struct svdm_svid_data {
	uint16_t svid;
	uint32_t mode_vdo[PDO_MODES];
};

struct svdm_amode_data {
	uint16_t svid;
	enum dfp_amode amode;
	uint32_t *mode_caps;
	void (*enter)(uint32_t mode_caps);
	void (*exit)(void);
};

/* Policy structure for driving alternate mode */
struct pd_policy {
	/* index of svid currently being operated on */
	int svid_idx;
	/* count of svids discovered */
	int svid_cnt;
	/* supported svids & corresponding vdo mode data */
	struct svdm_svid_data svids[SVID_DISCOVERY_MAX];
	/* index of amode currently being operated on */
	int amode_idx;
	/* count of amodes discovered */
	int amode_cnt;
	/* supported amodes */
	struct svdm_amode_data *amodes;
};

/*
 * VDO : Vendor Defined Message Object
 * VDM object is minimum of VDM header + 6 additional data objects.
 */

/*
 * VDM header
 * ----------
 * <31:16>  :: SVID
 * <15>     :: VDM type ( 1b == structured, 0b == unstructured )
 * <14:13>  :: Structured VDM version
 * <12:11>  :: reserved
 * <10:8>   :: object position (1-7 valid ... used for enter/exit mode only)
 * <7:6>    :: command type (SVDM only?)
 * <5>      :: reserved (SVDM), command type (UVDM)
 * <4:0>    :: command
 */
#define VDO_MAX_SIZE 7
#define VDO(vid, type, custom)				\
	(((vid) << 16) |				\
	 ((type) << 15) |				\
	 ((custom) & 0x7FFF))

#define VDO_SVDM_TYPE     (1 << 15)
#define VDO_SVDM_VERS(x)  (x << 13)
#define VDO_OPOS(x)       (x << 8)
#define VDO_CMDT(x)       (x << 6)
#define VDO_CMDT_MASK     VDO_CMDT(0x3)

#define CMDT_INIT     0
#define CMDT_RSP_NAK  1
#define CMDT_RSP_ACK  2
#define CMDT_RSP_BUSY 3


/* reserved for SVDM ... for Google UVDM */
#define VDO_SRC_INITIATOR (0 << 5)
#define VDO_SRC_RESPONDER (1 << 5)

#define CMD_DISCOVER_IDENT 1
#define CMD_DISCOVER_SVID  2
#define CMD_DISCOVER_MODES 3
#define CMD_ENTER_MODE     4
#define CMD_EXIT_MODE      5
#define CMD_ATTENTION      6
#define VDO_CMD_VENDOR(x)    (((10 + (x)) & 0x1f))

/* ChromeOS specific commands */
#define VDO_CMD_VERSION      VDO_CMD_VENDOR(0)
#define VDO_CMD_SEND_INFO    VDO_CMD_VENDOR(1)
#define VDO_CMD_READ_INFO    VDO_CMD_VENDOR(2)
#define VDO_CMD_REBOOT       VDO_CMD_VENDOR(5)
#define VDO_CMD_FLASH_ERASE  VDO_CMD_VENDOR(6)
#define VDO_CMD_FLASH_WRITE  VDO_CMD_VENDOR(7)
#define VDO_CMD_ERASE_SIG    VDO_CMD_VENDOR(8)
#define VDO_CMD_PING_ENABLE  VDO_CMD_VENDOR(10)
#define VDO_CMD_CURRENT      VDO_CMD_VENDOR(11)

#define PD_VDO_VID(vdo)  ((vdo) >> 16)
#define PD_VDO_SVDM(vdo) (((vdo) >> 15) & 1)
#define PD_VDO_OPOS(vdo) (((vdo) >> 8) & 0x7)
#define PD_VDO_CMD(vdo)  ((vdo) & 0x1f)
#define PD_VDO_CMDT(vdo) (((vdo) >> 6) & 0x3)

/*
 * SVDM Identity request -> response
 *
 * Request is simply properly formatted SVDM header
 *
 * Response is 4 data objects:
 * [0] :: SVDM header
 * [1] :: Identitiy header
 * [2] :: Cert Stat VDO
 * [3] :: (Product | Cable | AMA) VDO
 *
 */
#define VDO_INDEX_HDR   0
#define VDO_INDEX_IDH   1
#define VDO_INDEX_CSTAT 2
#define VDO_INDEX_CABLE 3
#define VDO_INDEX_AMA   3
#define VDO_I(name) VDO_INDEX_##name

/*
 * SVDM Identity Header
 * --------------------
 * <31>     :: data capable as a USB host
 * <30>     :: data capable as a USB device
 * <29:27>  :: product type
 * <26>     :: modal operation supported (1b == yes)
 * <25:16>  :: SBZ
 * <15:0>   :: USB-IF assigned VID for this cable vendor
 */
#define IDH_PTYPE_UNDEF  0
#define IDH_PTYPE_HUB    1
#define IDH_PTYPE_PERIPH 2
#define IDH_PTYPE_ACABLE 4
#define IDH_PTYPE_PCABLE 5
#define IDH_PTYPE_AMA    6

#define VDO_IDH(usbh, usbd, ptype, is_modal, vid)		\
	((usbh) << 31 | (usbd) << 30 | ((ptype) & 0x7) << 27	\
	 | (is_modal) << 26 | ((vid) & 0xffff))

#define PD_IDH_PTYPE(vdo) (((vdo) >> 27) & 0x7)
#define PD_IDH_VID(vdo)   ((vdo) & 0xffff)

/*
 * Cert Stat VDO
 * -------------
 * <31:20> : SBZ
 * <19:0>  : USB-IF assigned TID for this cable
 */
#define VDO_CSTAT(tid)    ((tid) & 0xfffff)
#define PD_CSTAT_TID(vdo) ((vdo) & 0xfffff)

/*
 * Cable VDO
 * ---------
 * <31:28> :: Cable HW version
 * <27:24> :: Cable FW version
 * <23:20> :: SBZ
 * <19:18> :: type-C to Type-A/B/C (00b == A, 01 == B, 10 == C)
 * <17>    :: Type-C to Plug/Receptacle (0b == plug, 1b == receptacle)
 * <16:13> :: cable latency (0001 == <10ns(~1m length))
 * <12:11> :: cable termination type (11b == both ends active VCONN req)
 * <10>    :: SSTX1 Directionality support (0b == fixed, 1b == cfgable)
 * <9>     :: SSTX2 Directionality support
 * <8>     :: SSRX1 Directionality support
 * <7>     :: SSRX2 Directionality support
 * <6:5>   :: Vbus current handling capability
 * <4>     :: Vbus through cable (0b == no, 1b == yes)
 * <3>     :: SOP" controller present? (0b == no, 1b == yes)
 * <2:0>   :: USB SS Signaling support
 */
#define CABLE_ATYPE 0
#define CABLE_BTYPE 1
#define CABLE_CTYPE 2
#define CABLE_PLUG       0
#define CABLE_RECEPTACLE 1
#define VDO_CABLE(hw, fw, cbl, gdr, lat, term, tx1d, tx2d, rx1d, rx2d, cur, vps, sopp, usbss) \
	(((hw) & 0x7) << 28 | ((fw) & 0x7) << 24 | ((cbl) & 0x3) << 18	\
	 | (gdr) << 17 | ((lat) & 0x7) << 13 | ((term) & 0x3) << 11	\
	 | (tx1d) << 10 | (tx2d) << 9 | (rx1d) << 8 | (rx2d) << 7	\
	 | ((cur) & 0x3) << 5 | (vps) << 4 | (sopp) << 3		\
	 | ((usbss) & 0x7))

/*
 * AMA VDO
 * ---------
 * <31:28> :: Cable HW version
 * <27:24> :: Cable FW version
 * <23:12> :: SBZ
 * <11>    :: SSTX1 Directionality support (0b == fixed, 1b == cfgable)
 * <10>    :: SSTX2 Directionality support
 * <9>     :: SSRX1 Directionality support
 * <8>     :: SSRX2 Directionality support
 * <7:5>   :: Vconn power
 * <4>     :: Vconn power required
 * <3>     :: Vbus power required
 * <2:0>   :: USB SS Signaling support
 */
#define VDO_AMA(hw, fw, tx1d, tx2d, rx1d, rx2d, vcpwr, vcr, vbr, usbss) \
	(((hw) & 0x7) << 28 | ((fw) & 0x7) << 24			\
	 | (tx1d) << 11 | (tx2d) << 10 | (rx1d) << 9 | (rx2d) << 8	\
	 | ((vcpwr) & 0x3) << 5 | (vcr) << 4 | (vbr) << 3		\
	 | ((usbss) & 0x7))

#define PD_VDO_AMA_VCONN_REQ(vdo) (((vdo) >> 4) & 1)
#define PD_VDO_AMA_VBUS_REQ(vdo)  (((vdo) >> 3) & 1)

/*
 * SVDM Discover SVIDs request -> response
 *
 * Request is properly formatted VDM Header with discover SVIDs command.
 * Response is a set of SVIDs of all all supported SVIDs with all zero's to
 * mark the end of SVIDs.  If more than 12 SVIDs are supported command SHOULD be
 * repeated.
 */
#define VDO_SVID(svid0, svid1) (((svid0) & 0xffff) << 16 | ((svid1) & 0xffff))
#define PD_VDO_SVID_SVID0(vdo) ((vdo) >> 16)
#define PD_VDO_SVID_SVID1(vdo) ((vdo) & 0xffff)

/*
 * Mode Capabilities
 *
 * Number of VDOs supplied is SID dependent (but <= 6 VDOS?)
 */
#define VDO_MODE_CNT_DISPLAYPORT 1

/*
 * DisplayPort modes capabilities
 * -------------------------------
 * <31:24> : SBZ
 * <23:16> : sink pin assignment supported
 * <15:8>  : source pin assignment supported
 * <7>     : USB 2.0 signaling (0b=yes, 1b=no)
 * <6>     : Plug | Receptacle (0b == plug, 1b == receptacle)
 * <5:3>   : USB Gen 2 signaling for DP (000b=no, 001b=yes, rest=rsv)
 * <2>     : supports dp1.3
 * <1:0>   : signal direction ( 00b=rsv, 01b=sink, 10b=src 11b=both )
 */
#define VDO_MODE_DP(snkp, srcp, usb, gdr, usbdp, dp3, sdir)		\
	(((snkp) & 0xff) << 16 | ((srcp) & 0xff) << 8			\
	 | ((usb) & 1) << 7 | ((gdr) & 1) << 6 | ((usbdp) & 0x7) << 3	\
	 | ((dp3) & 1) << 2 | ((sdir) & 0x3))

#define MODE_DP_PIN_A 0x01
#define MODE_DP_PIN_B 0x02
#define MODE_DP_PIN_C 0x04
#define MODE_DP_PIN_D 0x08
#define MODE_DP_PIN_E 0x10

#define MODE_DP_SNK  0x1
#define MODE_DP_SRC  0x2
#define MODE_DP_BOTH 0x3

/*
 * ChromeOS specific PD device Hardware IDs. Used to identify unique
 * products and used in VDO_INFO. Note this field is 10 bits.
 */
#define USB_PD_HW_DEV_ID_RESERVED    0
#define USB_PD_HW_DEV_ID_ZINGER      1
#define USB_PD_HW_DEV_ID_MINIMUFFIN  2

/*
 * ChromeOS specific VDO_CMD_READ_INFO responds with device info including:
 * RW Hash: First 20 bytes of SHA-256 of RW (20 bytes)
 * HW Device ID: unique descriptor for each ChromeOS model (2 bytes)
 *               top 6 bits are minor revision, bottom 10 bits are major
 * SW Debug Version: Software version useful for debugging (15 bits)
 * IS RW: True if currently in RW, False otherwise (1 bit)
 */
#define VDO_INFO(id, id_minor, ver, is_rw) ((id_minor) << 26 \
				  | ((id) & 0x3ff) << 16 \
				  | ((ver) & 0x7fff) << 1 \
				  | ((is_rw) & 1))
#define VDO_INFO_HW_DEV_ID(x)    ((x) >> 16)
#define VDO_INFO_SW_DBG_VER(x)   (((x) >> 1) & 0x7fff)
#define VDO_INFO_IS_RW(x)        ((x) & 1)

/* USB-IF SIDs */
#define USB_SID_PD          0xff00 /* power delivery */
#define USB_SID_DISPLAYPORT 0xff01
/* USB Vendor ID assigned to Google Inc. */
#define USB_VID_GOOGLE 0x18d1

/* Timeout for message receive in microseconds */
#define USB_PD_RX_TMOUT_US 2700

/* --- Protocol layer functions --- */

enum pd_states {
	PD_STATE_DISABLED,
#ifdef CONFIG_USB_PD_DUAL_ROLE
	PD_STATE_SUSPENDED,
	PD_STATE_SNK_DISCONNECTED,
	PD_STATE_SNK_DISCOVERY,
	PD_STATE_SNK_REQUESTED,
	PD_STATE_SNK_TRANSITION,
	PD_STATE_SNK_READY,
#endif /* CONFIG_USB_PD_DUAL_ROLE */

	PD_STATE_SRC_DISCONNECTED,
	PD_STATE_SRC_DISCOVERY,
	PD_STATE_SRC_NEGOCIATE,
	PD_STATE_SRC_ACCEPTED,
	PD_STATE_SRC_TRANSITION,
	PD_STATE_SRC_READY,

	PD_STATE_SOFT_RESET,
	PD_STATE_HARD_RESET,
	PD_STATE_BIST,

	/* Number of states. Not an actual state. */
	PD_STATE_COUNT,
};

#ifdef CONFIG_USB_PD_DUAL_ROLE
enum pd_dual_role_states {
	PD_DRP_TOGGLE_ON,
	PD_DRP_TOGGLE_OFF,
	PD_DRP_FORCE_SINK,
	PD_DRP_FORCE_SOURCE
};
/**
 * Set dual role state, from among enum pd_dual_role_states
 *
 * @param state New state of dual-role port, selected from
 *              enum pd_dual_role_states
 */
void pd_set_dual_role(enum pd_dual_role_states state);
#endif

/* Control Message type */
enum pd_ctrl_msg_type {
	/* 0 Reserved */
	PD_CTRL_GOOD_CRC = 1,
	PD_CTRL_GOTO_MIN = 2,
	PD_CTRL_ACCEPT = 3,
	PD_CTRL_REJECT = 4,
	PD_CTRL_PING = 5,
	PD_CTRL_PS_RDY = 6,
	PD_CTRL_GET_SOURCE_CAP = 7,
	PD_CTRL_GET_SINK_CAP = 8,
	PD_CTRL_PROTOCOL_ERR = 9,
	PD_CTRL_SWAP = 10,
	/* 11 Reserved */
	PD_CTRL_WAIT = 12,
	PD_CTRL_SOFT_RESET = 13,
	/* 14-15 Reserved */
};

/* Data message type */
enum pd_data_msg_type {
	/* 0 Reserved */
	PD_DATA_SOURCE_CAP = 1,
	PD_DATA_REQUEST = 2,
	PD_DATA_BIST = 3,
	PD_DATA_SINK_CAP = 4,
	/* 5-14 Reserved */
	PD_DATA_VENDOR_DEF = 15,
};

/* Protocol revision */
#define PD_REV10 0

/* BMC-supported bit : we are using the baseband variant of the protocol */
#define PD_BMC_SUPPORTED (1 << 15)

/* Port role */
#define PD_ROLE_SINK   0
#define PD_ROLE_SOURCE 1

/* build message header */
#define PD_HEADER(type, role, id, cnt) \
	((type) | (PD_REV10 << 6) | \
	 ((role) << 8) | ((id) << 9) | ((cnt) << 12) | \
	 PD_BMC_SUPPORTED)

#define PD_HEADER_CNT(header)  (((header) >> 12) & 7)
#define PD_HEADER_TYPE(header) ((header) & 0xF)
#define PD_HEADER_ID(header)   (((header) >> 9) & 7)

/* K-codes for special symbols */
#define PD_SYNC1 0x18
#define PD_SYNC2 0x11
#define PD_RST1  0x07
#define PD_RST2  0x19
#define PD_EOP   0x0D

/* --- Policy layer functions --- */

/**
 * Decide which voltage to use from the source capabilities.
 *
 * @param cnt  the number of Power Data Objects.
 * @param src_caps Power Data Objects representing the source capabilities.
 * @param rdo  requested Request Data Object.
 * @return <0 if invalid, else value is the current limit of the RDO data
 */
int pd_choose_voltage(int cnt, uint32_t *src_caps, uint32_t *rdo);

/**
 * Put a cap on the max voltage requested as a sink.
 * @param mv maximum voltage in millivolts.
 */
void pd_set_max_voltage(unsigned mv);

/**
 * Request a new operating voltage.
 *
 * @param rdo  Request Data Object with the selected operating point.
 * @return EC_SUCCESS if we can get the requested voltage/OP, <0 else.
 */
int pd_request_voltage(uint32_t rdo);

/**
 * Go back to the default/safe state of the power supply
 *
 * @param port USB-C port number
 */
void pd_power_supply_reset(int port);

/**
 * Enable the power supply output after the ready delay.
 *
 * @param port USB-C port number
 * @return EC_SUCCESS if the power supply is ready, <0 else.
 */
int pd_set_power_supply_ready(int port);

/**
 * Ask the specified voltage from the PD source.
 *
 * It triggers a new negotiation sequence with the source.
 * @param port USB-C port number
 * @param mv request voltage in millivolts.
 */
void pd_request_source_voltage(int port, int mv);

/**
 * Set the input current limit.
 *
 * @max_ma Maximum current limit
 */
void pd_set_input_current_limit(uint32_t max_ma);

/**
 * Verify board specific health status : current, voltages...
 *
 * @return EC_SUCCESS if the board is good, <0 else.
 */
int pd_board_checks(void);

/**
 * Get PD device info used for VDO_CMD_SEND_INFO / VDO_CMD_READ_INFO
 *
 * @return Pointer to data payload for VDO_CMD_*_INFO
 */
uint32_t *pd_get_info(void);

/**
 * Handle Vendor Defined Messages
 *
 * @param port     USB-C port number
 * @param cnt      number of data objects in the payload.
 * @param payload  payload data.
 * @param rpayload pointer to the data to send back.
 * @return if >0, number of VDOs to send back.
 */
int pd_vdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload);

/**
 * Handle Structured Vendor Defined Messages
 *
 * @param port     USB-C port number
 * @param cnt      number of data objects in the payload.
 * @param payload  payload data.
 * @param rpayload pointer to the data to send back.
 * @return if >0, number of VDOs to send back.
 */
int pd_svdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload);

/**
 * Choose appropriate alternate modes.
 *
 * @param pe        pd_policy data structure
 */
void pd_dfp_choose_modes(struct pd_policy *pe);

/**
 * Exit alternate mode
 *
 * @param port     USB-C port number
 * @param payload  payload data.
 * @return if >0, number of VDOs to send back.
 */
int pd_exit_modes(int port, uint32_t *payload);

/**
 * Store Device ID & RW hash of device
 *
 * @param port     USB-C port number
 * @param dev_id   device identifier
 * @param rw_hash  pointer to rw_hash
 */
void pd_dev_store_rw_hash(int port, uint16_t dev_id, uint32_t *rw_hash);

/**
 * Send Vendor Defined Message
 *
 * @param port     USB-C port number
 * @param vid      Vendor ID
 * @param cmd      VDO command number
 * @param data     Pointer to payload to send
 * @param data     number of data objects in payload
 */
void pd_send_vdm(int port, uint32_t vid, int cmd, const uint32_t *data,
		 int count);

/* Power Data Objects for the source and the sink */
extern const uint32_t pd_src_pdo[];
extern const int pd_src_pdo_cnt;
extern const uint32_t pd_snk_pdo[];
extern const int pd_snk_pdo_cnt;

/**
 * Get PD source power data objects.
 *
 * @param src_pdo pointer to the data to return.
 * @return number of PDOs returned.
 */
int pd_get_source_pdo(const uint32_t **src_pdo);

/* Muxing for the USB type C */
enum typec_mux {
	TYPEC_MUX_NONE,
	TYPEC_MUX_USB,
	TYPEC_MUX_DP,
	TYPEC_MUX_DOCK,
};

/**
 * Configure superspeed muxes on type-C port.
 *
 * @param port port number.
 * @param mux selected function.
 * @param polarity plug polarity (0=CC1, 1=CC2).
 */
void board_set_usb_mux(int port, enum typec_mux mux, int polarity);

/**
 * Query superspeed mux status on type-C port.
 *
 * @param port port number.
 * @param dp_str pointer to the DP string to return.
 * @param usb_str pointer to the USB string to return.
 * @return Non-zero if superspeed connection is enabled; otherwise, zero.
 */
int board_get_usb_mux(int port, const char **dp_str, const char **usb_str);

/* --- Physical layer functions : chip specific --- */

/* Packet preparation/retrieval */

/**
 * Prepare packet reading state machine.
 *
 * @param port USB-C port number
 */
void pd_init_dequeue(int port);

/**
 * Prepare packet reading state machine.
 *
 * @param port USB-C port number
 * @param off  current position in the packet buffer.
 * @param len  minimum size to read in bits.
 * @param val  the read bits.
 * @return new position in the packet buffer.
 */
int pd_dequeue_bits(int port, int off, int len, uint32_t *val);

/**
 * Advance until the end of the preamble.
 *
 * @param port USB-C port number
 * @return new position in the packet buffer.
 */
int pd_find_preamble(int port);

/**
 * Write the preamble in the TX buffer.
 *
 * @param port USB-C port number
 * @return new position in the packet buffer.
 */
int pd_write_preamble(int port);

/**
 * Write one 10-period symbol in the TX packet.
 * corresponding to a quartet with 4b5b encoding
 * and Biphase Mark Coding.
 *
 * @param port USB-C port number
 * @param bit_off current position in the packet buffer.
 * @param val10    the 10-bit integer.
 * @return new position in the packet buffer.
 */
int pd_write_sym(int port, int bit_off, uint32_t val10);


/**
 * Ensure that we have an edge after EOP and we end up at level 0,
 * also fill the last byte.
 *
 * @param port USB-C port number
 * @param bit_off current position in the packet buffer.
 * @return new position in the packet buffer.
 */
int pd_write_last_edge(int port, int bit_off);

/**
 * Dump the current PD packet on the console for debug.
 *
 * @param port USB-C port number
 * @param msg  context string.
 */
void pd_dump_packet(int port, const char *msg);

/**
 * Change the TX data clock frequency.
 *
 * @param port USB-C port number
 * @param freq frequency in hertz.
 */
void pd_set_clock(int port, int freq);

/* TX/RX callbacks */

/**
 * Start sending over the wire the prepared packet.
 *
 * @param port USB-C port number
 * @param polarity plug polarity (0=CC1, 1=CC2).
 * @param bit_len size of the packet in bits.
 */
void pd_start_tx(int port, int polarity, int bit_len);

/**
 * Set PD TX DMA to use circular mode. Call this before pd_start_tx() to
 * continually loop over the transmit buffer given in pd_start_tx().
 *
 * @param port USB-C port number
 */
void pd_tx_set_circular_mode(int port);

/**
 * Call when we are done sending a packet.
 *
 * @param port USB-C port number
 * @param polarity plug polarity (0=CC1, 1=CC2).
 */
void pd_tx_done(int port, int polarity);

/**
 * Check whether the PD reception is started.
 *
 * @param port USB-C port number
 * @return true if the reception is on-going.
 */
int pd_rx_started(int port);

/**
 * Suspend the PD task.
 * @param port USB-C port number
 * @param enable pass 0 to resume, anything else to suspend
 */
void pd_set_suspend(int port, int enable);

/* Callback when the hardware has detected an incoming packet */
void pd_rx_event(int port);
/* Start sampling the CC line for reception */
void pd_rx_start(int port);
/* Call when we are done reading a packet */
void pd_rx_complete(int port);

/* restart listening to the CC wire */
void pd_rx_enable_monitoring(int port);
/* stop listening to the CC wire during transmissions */
void pd_rx_disable_monitoring(int port);

/**
 * Deinitialize the hardware used for PD.
 *
 * @param port USB-C port number
 */
void pd_hw_release(int port);

/**
 * Initialize the hardware used for PD RX/TX.
 *
 * @param port USB-C port number
 */
void pd_hw_init(int port);

/* --- Protocol layer functions --- */
/**
 * Get connected state
 *
 * @param port USB-C port number
 * @return True if port is in connected state
 */
int pd_is_connected(int port);

/**
 * Get port polarity.
 *
 * @param port USB-C port number
 */
int pd_get_polarity(int port);

/**
 * Set the PD communication enabled flag. When communication is disabled,
 * the port can still detect connection and source power but will not
 * send or respond to any PD communication.
 *
 * @param enable Enable flag to set
 */
void pd_comm_enable(int enable);

/**
 * Set the PD pings enabled flag. When source has negotiated power over
 * PD successfully, it can optionally send pings periodically based on
 * this enable flag.
 *
 * @param port USB-C port number
 * @param enable Enable flag to set
 */
void pd_ping_enable(int port, int enable);

/* Issue PD soft reset */
void pd_soft_reset(void);

#endif  /* __USB_PD_H */

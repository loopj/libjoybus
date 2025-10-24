// Report IDs
#define RPT_SET_MOTOR             0x11
#define RPT_GET_ORIGIN            0x12
#define RPT_ENABLE_POLLING        0x13
#define RPT_DISABLE_POLLING       0x14
#define RPT_RESET                 0x15
#define RPT_STATE                 0x21
#define RPT_ORIGIN                0x22
#define RPT_POLLING_ENABLED       0x23
#define RPT_POLLING_DISABLED      0x24
#define RPT_RESET_ACK             0x25

// Report lengths (including report ID)
#define RPT_SET_MOTOR_LEN         5
#define RPT_GET_ORIGIN_LEN        1
#define RPT_ENABLE_POLLING_LEN    1
#define RPT_DISABLE_POLLING_LEN   1
#define RPT_RESET_LEN             1
#define RPT_STATE_LEN             37
#define RPT_ORIGIN_LEN            25
#define RPT_POLLING_ENABLED_LEN   2
#define RPT_POLLING_DISABLED_LEN  2
#define RPT_RESET_ACK_LEN         2
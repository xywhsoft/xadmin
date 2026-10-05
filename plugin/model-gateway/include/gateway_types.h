/* Fixed owned request snapshots. Never borrow a live row/config/key across I/O. */
typedef struct GatewayRates {
    int64_t input,cache_read,cache_write_5m,cache_write_1h,output;
} GatewayRates;
typedef struct GatewayPrice {
    int64_t version;
    GatewayRates sale,cost;
    bool cost_known;
} GatewayPrice;
typedef struct GatewayChannel {
    char id[65],title[129],provider[65],protocol[17],url[2049],auth[17],anthropic_version[33];
    int timeout_ms,first_byte_ms,idle_ms,max_concurrent;
    bool enabled,allow_http;
} GatewayChannel;
typedef struct GatewayModel {
    char id[129],title[129],description[513],reasoning[129],output_field[33],default_protocol[17];
    int context_window,max_output;
    bool enabled,member_only,tool_calling,vision;
    GatewayPrice price;
} GatewayModel;
typedef struct GatewayUsage {
    int64_t input,cache_read,cache_write_5m,cache_write_1h,output,reasoning;
    bool input_known,output_known,authoritative,terminal,invalid;
} GatewayUsage;
typedef struct GatewayCall {
    XS_RequestObject req;
    GatewayModel model;
    GatewayChannel channel;
    GatewayUsage usage;
    char id[33],wire_model[129],protocol[17],provider_request_id[129],error[65];
    int discount_bps,output_limit,upstream_status;
    int64_t owner,reserved,started_us,first_byte_us;
    bool stream,started,sent,sse,upstream_error;
    xbuffer line,event,json;
    xvalue* reported_usage;
    bool previous_cr,sse_first_line;
} GatewayCall;
typedef struct GatewayConfig {int max_concurrent,minute_limit,daily_limit,global_daily_limit;int64_t daily_budget;} GatewayConfig;
typedef struct GatewayActive {int64_t owner;char channel[65];} GatewayActive;

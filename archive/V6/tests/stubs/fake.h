#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#define ARG_UNUSED(x) (void)(x)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define K_MSEC(x) (x)
#define K_NO_WAIT 0
struct k_work { int unused; };
struct k_work_delayable { void (*fn)(struct k_work *); int64_t due; };
struct k_spinlock { int unused; };
typedef int k_spinlock_key_t;
typedef int atomic_t;
static int64_t clock_ms;
static int64_t k_uptime_get(void) { return clock_ms; }
static int k_spin_lock(struct k_spinlock *p) { (void)p; return 0; }
static void k_spin_unlock(struct k_spinlock *p,int key) {(void)p;(void)key;}
static void k_work_init_delayable(struct k_work_delayable *w,void (*f)(struct k_work*)) {w->fn=f;}
static int k_work_reschedule(struct k_work_delayable *w,int64_t d) {w->due=clock_ms+d;return 0;}
static int k_work_schedule(struct k_work_delayable *w,int64_t d) {return k_work_reschedule(w,d);}
static int atomic_cas(int *p,int a,int b) {if(*p!=a)return 0;*p=b;return 1;}
static void atomic_clear(int *p){*p=0;}
struct gpio_dt_spec {int x;};
#define GPIO_DT_SPEC_GET(a,b) {0}
#define GPIO_OUTPUT_INACTIVE 0
static bool gpio_is_ready_dt(const struct gpio_dt_spec *p){(void)p;return true;}
static int gpio_pin_configure_dt(const struct gpio_dt_spec *p,int v){(void)p;(void)v;return 0;}
static int gpio_pin_set_dt(const struct gpio_dt_spec *p,int v){(void)p;(void)v;return 0;}
typedef uint8_t zb_bufid_t;
typedef uint8_t zb_uint8_t;
typedef uint16_t zb_uint16_t;
typedef int zb_ret_t;
typedef union {uint16_t addr_short;} zb_addr_u;
typedef struct {int status;} zb_zcl_command_send_status_t;
#define RET_OK 0
#define ZB_APS_ADDR_MODE_16_ENDP_PRESENT 2
#define ZB_AF_HA_PROFILE_ID 0x104
#define ZB_ZCL_CLUSTER_ID_ON_OFF 6
#define ZB_ZCL_CMD_ON_OFF_TOGGLE_ID 2
#define ZB_ZCL_DISABLE_DEFAULT_RESPONSE 1
static bool joined=true, have_buffer=true;
static int schedule_rc, send_rc, sends, frees;
static uint8_t packet[8], seq;
static zb_zcl_command_send_status_t fake_status;
static void (*completion)(zb_bufid_t);
#define ZB_JOINED() joined
#define ZB_BUF_GET_PARAM(b,t) (&fake_status)
#define ZB_ZCL_START_PACKET_REQ(b) packet
#define ZB_ZCL_CONSTRUCT_SPECIFIC_COMMAND_REQ_FRAME_CONTROL(p,d) do {*(p)++=1|((d)<<4);} while(0)
#define ZB_ZCL_GET_SEQ_NUM() seq++
#define ZB_ZCL_CONSTRUCT_COMMAND_HEADER_REQ(p,s,c) do {*(p)++=(s);*(p)++=(c);} while(0)
#define ZB_SCHEDULE_APP_CALLBACK2(f,a,b) schedule_rc
static void user_input_indicate(void) {}
static zb_bufid_t zb_buf_get_out(void) {return have_buffer?1:0;}
static void zb_buf_free(zb_bufid_t b){assert(b==1);frees++;}
static int zb_zcl_finish_and_send_packet(zb_bufid_t b,uint8_t *p,const zb_addr_u *dst,int mode,int dep,int sep,int profile,int cluster,void (*cb)(zb_bufid_t)){
 assert(b==1 && p-packet==3 && dst->addr_short==0 && mode==2 && dep==1 && sep==1 && profile==0x104 && cluster==6);
 assert(packet[0]==0x11 && packet[2]==2);
 completion=cb;sends++;return send_rc;
}

#define ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC 1
#define ZB_ZCL_NOT_MANUFACTURER_SPECIFIC 0
#define ZB_ZCL_FRAME_DIRECTION_TO_SRV 0
#define ZB_ZCL_CONSTRUCT_FRAME_CONTROL(t,m,d,r) ((t)|((m)<<2)|((d)<<3)|((r)<<4))
static uint8_t *zb_zcl_start_command_header(zb_bufid_t b,int fc,int manufacturer,int cmd,void *tsn) {
 assert(b==1 && manufacturer==0 && tsn==NULL);packet[0]=fc;packet[1]=seq++;packet[2]=cmd;return packet+3;
}


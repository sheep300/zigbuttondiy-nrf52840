#include <stdio.h>
#include "../src/battery_curve.h"
#include "../src/toggle.c"
#include "../src/pairing.c"
static void reset(void) {
 memset(&queue,0,sizeof(queue)); busy=false;timed_out=false;callback_pending=0;
 clock_ms=0;joined=true;have_buffer=true;schedule_rc=send_rc=sends=frees=0;
 toggle_enqueued=toggle_overflow=toggle_expired=toggle_no_network=0;
 toggle_confirmed=toggle_failed=toggle_timeouts=toggle_submitted=0;
 toggle_init();
}
int main(void) {
 reset(); toggle_enqueue(); pump_handler(NULL); assert(callback_pending); pump_zigbee(0,0);
 assert(sends==1 && busy && !queue.count); fake_status.status=0;completion(1);
 assert(frees==1 && toggle_confirmed==1 && !busy);pump_handler(NULL);assert(sends==1);
 reset();toggle_enqueue();pump_zigbee(0,0);fake_status.status=42;completion(1);pump_handler(NULL);
 assert(toggle_failed==1 && sends==1 && !queue.count); /* no duplicate after uncertain delivery */
 reset();have_buffer=false;toggle_enqueue();pump_zigbee(0,0);assert(queue.count==1 && !sends);
 have_buffer=true;clock_ms=100;pump_zigbee(0,0);assert(sends==1 && !queue.count);
 reset();have_buffer=false;toggle_enqueue();clock_ms=3000;pump_handler(NULL);assert(!queue.count && toggle_expired==1 && !sends);
 reset();schedule_rc=-1;toggle_enqueue();pump_handler(NULL);assert(!callback_pending && queue.count==1);
 schedule_rc=0;clock_ms=100;pump_handler(NULL);pump_zigbee(0,0);assert(sends==1);
 reset();toggle_enqueue();toggle_enqueue();pump_zigbee(0,0);pump_zigbee(0,0);assert(sends==1 && queue.count==1);
 fake_status.status=0;completion(1);pump_zigbee(0,0);assert(sends==2 && !queue.count);
 reset();joined=false;toggle_enqueue();pump_zigbee(0,0);assert(!sends && toggle_no_network==1 && !queue.count);
 reset();toggle_enqueue();pump_zigbee(0,0);clock_ms=15000;pump_handler(NULL);
 assert(timed_out && busy && toggle_timeouts==1 && frees==0);toggle_enqueue();pump_zigbee(0,0);assert(sends==1);
 fake_status.status=0;completion(1);pump_zigbee(0,0);assert(sends==2); /* late callback releases ownership */
 reset();for(int i=0;i<9;i++)toggle_enqueue();assert(queue.count==8 && toggle_overflow==1);
 reset();send_rc=-1;toggle_enqueue();pump_zigbee(0,0);assert(frees==1 && !busy && toggle_failed==1);
 struct toggle_queue q={0};for(int i=0;i<8;i++)assert(tq_push(&q,i*100));
 assert(!tq_push(&q,800));assert(tq_expire(&q,2999)==0);assert(tq_expire(&q,3100)==2);
 assert(q.count==6 && q.time[q.head]==200);assert(tq_push(&q,3200));assert(tq_push(&q,3300));
 assert(tq_expire(&q,6000)==6 && q.count==2 && q.time[q.head]==3200);
 assert(battery_percent_from_mv(2900)==0 && battery_percent_from_mv(4200)==100 && battery_percent_from_mv(4400)==100);
 assert(battery_percent_from_mv(3700)==25 && battery_percent_from_mv(4000)==80);
 for(int mv=2801;mv<=4400;mv++)assert(battery_percent_from_mv(mv)>=battery_percent_from_mv(mv-1) && battery_percent_from_mv(mv)<=100);

 struct gesture g;gesture_init(&g,0);
 for(int i=0;i<5;i++)assert(gesture_edge(&g,100+i*100)==G_NONE);
 assert(gesture_edge(&g,600)==G_ARMED);assert(gesture_edge(&g,700)==G_NONE);
 assert(gesture_edge(&g,800)==G_CONFIRMED);assert(gesture_edge(&g,900)==G_NONE);
 gesture_init(&g,0);for(int i=0;i<5;i++)gesture_edge(&g,100+i*100);
 assert(gesture_edge(&g,5000)==G_TOGGLE && g.phase==G_NORMAL);
 gesture_init(&g,0);for(int i=0;i<6;i++)gesture_edge(&g,100+i*100);
 assert(gesture_tick(&g,5600)==G_CANCELLED && g.phase==G_NORMAL);
 assert(gesture_edge(&g,5700)==G_TOGGLE);
 reset();stack_ready=reset_state=notice=0;reset_calls=abort_calls=0;pairing_init();
 for(int i=0;i<6;i++){clock_ms=100+i*100;assert(pairing_button());}
 assert(gesture.phase==G_CONFIRM && toggle_enqueued==0);
 clock_ms=700;assert(pairing_button());clock_ms=800;assert(pairing_button());
 assert(reset_state==1);reset_zigbee(0,0);assert(reset_calls==0);
 pairing_signal(ZB_ZDO_SIGNAL_SKIP_STARTUP,0);reset_zigbee(0,0);reset_zigbee(0,0);
 assert(reset_calls==1 && abort_calls==1 && reset_state==2);
 assert(pairing_signal(ZB_ZDO_SIGNAL_LEAVE,0));pairing_signal(ZB_BDB_SIGNAL_STEERING,0);
 notice_handler(NULL);assert(gesture.phase==G_NORMAL && reset_state==0);
 assert(!pairing_button());
 reset();pairing_init();reset_state=1;reset_deadline=500;clock_ms=500;
 reset_handler(NULL);reset_zigbee(0,0);assert(reset_state==0 && reset_calls==1);
 puts("PASS: reset gesture thresholds/deadlines/cancel, no gesture toggles, stack readiness, exactly one reset call, merged leave/join notices, expired request");
 puts("PASS: LiPo bounds/monotonicity;");
 puts("PASS: send format, callback success/failure, resource retry, expiry, scheduler retry, FIFO, network loss, timeout/late callback, overflow, immediate error");
 return 0;
}


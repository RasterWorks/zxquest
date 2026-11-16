/* Foreground pacing, separate from serialized AGI state. Never changes actor
   step sizes or skips a logic/animation cycle. Included by main and tests. */
typedef struct { uint16_t began,ended,wait; uint8_t speed; } cycle_pacer;
static uint8_t pace_ready(cycle_pacer *p,uint16_t now,uint8_t speed){
    if(speed!=p->speed)p->wait=0;
    return (uint16_t)(now-p->ended)>=p->wait;
}
static void pace_end(cycle_pacer *p,uint16_t now,uint8_t speed){
    uint16_t period,work=now-p->began;
    period=speed<=2?(uint16_t)speed*5:(uint16_t)(speed-1)*10;
    p->wait=work<period?period-work:0;
    /* Keep Normal distinct from Fastest when processing exceeds its target.
       One frame also keeps walking close to the pre-optimization cadence. */
    if(speed==2 && !p->wait)p->wait=1;
    /* Slow remains observably slower even in CPU-bound rooms. */
    if(speed>2 && p->wait<10)p->wait=10;
    p->ended=now;p->speed=speed;
}

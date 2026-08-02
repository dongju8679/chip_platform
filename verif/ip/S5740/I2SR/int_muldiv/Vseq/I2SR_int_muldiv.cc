/* I2SR_int_muldiv.cc - rt-dev original host Vseq, copied as-is.
 *
 * Formatting, brace structure and control flow are rt-dev's, untouched. In particular the
 * per-round rhythm is preserved: check() and initialization() run INSIDE the outer loop, so
 * every OUTER_ITERATION verifies its own 50 interrupts against a freshly seeded BUF.
 *
 * Only these were changed, because the file cannot compile / cannot pass otherwise:
 *   1. OFFSET()  - one closing paren was missing            (compile error)
 *   2. tikc(100) -> tick(100)                               (compile error)
 *   3. if (data1!=ref_buf[i].data)[j][0]                    (compile error)
 *        -> if (data1!=ref_buf[i].data[j][0])
 *   4. initialization() wrote data[j][0] twice - the second write zeroed the seed it had
 *      just stored, so check()'s seed comparison could never pass. Changed the second one
 *      to data[j][1], which is the accumulator the ISRs add into.
 *      * Confirm this one against the real rt-dev source - it may be a transcription slip.
 *
 * master_write/master_read/tick/trigger_irq/host_set_bp/host_wait_bp come from
 * S5740_mcu_tests.h, exactly as on rt-dev. Nothing else is adapted.
 */
#include "S5740_mcu_tests.h"
#include "I2SR_int_muldiv.h"

static struct BUF ref_buf[8];

#define OFFSET(TYPE, MEMBER) ((unsigned int)(size_t)(& (((struct TYPE *)0)->MEMBER)))

bool I2SR_int_muldiv_check()
{
    bool pass = true;
    for(int i =1;i < 8;i++) {
        unsigned int data1, data2;
        unsigned int addr = BUF_ADDR + i * sizeof(struct BUF);
        master_read(addr+OFFSET(BUF, min_time), ref_buf[i].min_time);
        master_read(addr+OFFSET(BUF, max_time), ref_buf[i].max_time);
        master_read(addr+OFFSET(BUF, tot_time), ref_buf[i].tot_time);
        master_read(addr+OFFSET(BUF, count), data1);
        if (i==0) ref_buf[i].count = data1;
        else if(data1 != ref_buf[i].count) {
            pass=false;
        }


        for (int j = 0; j < BUF_SIZE; j++) {
            master_read(addr + OFFSET(BUF, data[j][0]), data1);

            if (data1!=ref_buf[i].data[j][0]) {
                return false;
            }

            master_read(addr+OFFSET(BUF,data[j][1]), data2);

            unsigned int buf = 0;
            for (int k = 0; k < ref_buf[i].count; k++) {
                buf += data1;
                buf /= 7;
                buf *= 3;

            }

            if (data2 != buf)
            {
                pass = false;

            }

        }
    }

    return pass;

}

void initialization() {
    srand((time(NULL)));
    for (int i = 0; i < 8; i++) {
        unsigned int addr = BUF_ADDR + i * sizeof(struct BUF);
        master_write(addr + OFFSET(BUF, min_time), 0xFFFFFFFF);
        master_write(addr + OFFSET(BUF, max_time), 0);
        master_write(addr + OFFSET(BUF, tot_time), 0);
        master_write(addr + OFFSET(BUF, count), 0);
        ref_buf[i].count = 0;
        for (int j = 0; j < BUF_SIZE; j++) {
#ifdef FIXED_INPUT
            ref_buf[i].data[j][0] = i<<8;
#else
            ref_buf[i].data[j][0] = rand();

#endif
            master_write(addr + OFFSET(BUF, data[j][0]), ref_buf[i].data[j][0]);
            master_write(addr + OFFSET(BUF, data[j][1]), 0);
        }

    }
}

bool I2SR_int_muldiv()
{
    initialization();
    host_set_bp(1);
    host_wait_bp(2);
    tick(100);
    for(int k = 1; k <= OUTER_ITERATION; k++) {

        unsigned int trigger[8] = {0,};
        for (int i = 1; i <= INNER_ITERATION; i++) {
            for(int j = 0; j < MAX_TASKNUM; j++) {
                unsigned int intr;
                do {
                    intr = rand() % 7 + 1;
                }while(trigger[intr] == i);
                trigger[intr] = i;
                ref_buf[intr].count++;

                if (intr == 6) {
                    master_write(SW_INT,1);
                }
                else if (intr == 7) {
                    master_write(MTIMECMP+4, 0);
                    master_write(MTIMECMP, 0);


                }
                else{
                    trigger_irq(1<<(intr-1));
                }
                tick(rand()%200);

            }
            if ((k==OUTER_ITERATION) & (i==INNER_ITERATION)) master_write(BREAK_ADDR,2);
            host_set_bp(3);
            host_wait_bp(4);
            tick(5000);

        }
        if(I2SR_int_muldiv_check() == false) return false;

        initialization();
        tick(100);

    }
    host_set_bp(5);
    master_write(BREAK_ADDR, 1);

    return true;
}

/* high precision and low uncertainty timer used for basic pulse sync 
 * implementation 
 *
 * Contributing authors: 
 * Nat Jeffries
 * Madhav Iyengar
 */

#include <nrk_timer.h>
#include <nrk_error.h>

volatile uint64_t current_time_ms;
void timer_0_callback();

/* reset current time in microseconds and initialize the timer interrupt */
uint8_t pulse_sync_timer_setup(){
	uint8_t timer = NRK_APP_TIMER_0;

	current_time_ms = 0;
	/* inc counter every ms */
	if (nrk_timer_int_configure(timer, 1, 0x3E80, timer_0_callback) != NRK_OK)
		return NRK_ERROR;
	return nrk_timer_int_start(timer);
}

void timer_0_callback(){
	current_time_ms += 1;
}

uint64_t pulse_sync_get_current_time(){
	/* protect from case where timer 3 output compare interrupt occurs between
	 * reading offset_ticks and current_time_ms */
	asm volatile( "CLI \n\n" );
	uint32_t offset_ticks = TCNT3;
	uint64_t result = (current_time_ms * 1000000L) + (offset_ticks * 62) + (offset_ticks >> 1);
	asm volatile( "SEI \n\n" );
	return result;
}

void pulse_sync_reset_timer(){
	current_time_ms = 0;
}

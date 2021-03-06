#include <rtthread.h>
#include <stdlib.h>
#include "controller.h"
#include "pid.h"
#include "pca9685.h"
#include "pwm_gen.h"

// #define FRM_DEBUG
#define LOG_TAG             "frm.semsor"
#include <frm_log.h>

#define EVENT_CNTLR_LOOP		(1<<0)
#define EVENT_CNTLR_RX_MSG		(1<<1)

static struct rt_timer timer_controller;
static struct rt_event event_controller;

static int camera_msg[2];
static char cam_tmp_d[8];
static char camera_msg_str[32];
static pid_ctrl_t pid_x;
static pid_ctrl_t pid_y;
static float error_x = 0;
static float error_y = 0;

static void timer_controller_update(void* parameter)
{
	rt_event_send(&event_controller, EVENT_CNTLR_LOOP);
}

// void camera_msg_update(const float *camera_new_msg)
void camera_msg_update(const char *camera_new_msg)
{
	rt_memcpy(camera_msg_str, camera_new_msg, 32);

	rt_event_send(&event_controller, EVENT_CNTLR_RX_MSG);
}

void controller_str2d()
{
	char *p = &camera_msg_str[0];
	int cnt = 0;
	int cam_cnt = 0;
	int digit_cnt = 0;
	int cnt_flag = 1;

	while (cam_cnt <= 2)
	{
		if (*p != 's')
		{
			if (*p == ' ')
			{
				// LOG_D("%c %d %d %c A", *p,cnt,digit_cnt,camera_msg_str[1]);
				rt_memcpy(cam_tmp_d,camera_msg_str+1,cnt*sizeof(char));
				cam_tmp_d[cnt] = '\0';
				// LOG_I(cam_tmp_d);
				camera_msg[0] = atoi(cam_tmp_d);
				// LOG_D("camera_msg[0]=%d",camera_msg[0]);
				cnt = 0;
				cam_cnt++;
				digit_cnt++;
				cnt_flag = 0;
			}
			if (*p == 'e')
			{
				// LOG_D("%c %d %d %c A", *p,cnt,digit_cnt,camera_msg_str[digit_cnt+1]);
				rt_memcpy(cam_tmp_d,camera_msg_str+1+digit_cnt,(cnt-1)*sizeof(char));
				cam_tmp_d[cnt-1] = '\0';
				cam_cnt++;
				// LOG_I(cam_tmp_d);
				camera_msg[1] = atoi(cam_tmp_d);
				// LOG_D("camera_msg[1]=%d",camera_msg[1]);
				cnt = 0;
				digit_cnt = 0;
				cnt_flag = 1;
				break;
			}
			cnt++;
			
			p++;
			if (cnt_flag)
			{
				digit_cnt++;
			}
		}
		else
		{
			p++;
		}
		
	}
	
}

void controller_loop(void)
{
	pid_x->in = (float)camera_msg[0];
	error_x = pid_x->in - pid_x->dest;
	pid_compute(pid_x, error_x);
	
	pid_y->in = (float)camera_msg[1];
	error_y = pid_y->in - pid_y->dest;
	pid_compute(pid_y, error_y);

	cntl_set_pwm(0,pid_x->out);
	cntl_set_pwm(1,pid_y->out);

	printf("%d %f %f %f %f %f %d ",camera_msg[0],pid_x->in,error_x,pid_x->out,pid_x->out_inc,pid_x->error,pid_x->dest);
	printf("| %d %f %f %f %f %f %d\n",camera_msg[1],pid_y->in,error_y,pid_y->out,pid_y->out_inc,pid_y->error,pid_y->dest);
}

void controller_entry(void *parameter)
{
	rt_err_t res;
	rt_uint32_t recv_set = 0;
	rt_uint32_t wait_set = EVENT_CNTLR_LOOP | EVENT_CNTLR_RX_MSG;

	/* initial codes .. */
	pid_x = rt_malloc(sizeof(struct pid_ctrl));
	pid_y = rt_malloc(sizeof(struct pid_ctrl));
	pid_init(pid_x);
	pid_init(pid_y);

	pid_set(pid_x, 71, PID_OUT_BAL_X);
	pid_set(pid_y, 78, PID_OUT_BAL_Y);
	// rt_kprintf("%d %p %p | ", sizeof(struct pid_ctrl),&pid_x,&pid_y);
	// printf("%d %d ",pid_x->dest,pid_x->out_bal);
	// printf("| %d %d\n",pid_y->dest,pid_y->out_bal);

	/* create event */
	res = rt_event_init(&event_controller, "event_controller", RT_IPC_FLAG_FIFO);

	/* register timer event */
	rt_timer_init(&timer_controller, "timer_controller",
					timer_controller_update,
					RT_NULL,
					6,
					RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
	rt_timer_start(&timer_controller);
	
	while(1)
	{
		res = rt_event_recv(&event_controller, wait_set, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 
								RT_WAITING_FOREVER, &recv_set);
		
		if(res == RT_EOK){
			if(recv_set & EVENT_CNTLR_LOOP){
				controller_loop();
			}
			if(recv_set & EVENT_CNTLR_RX_MSG){
				controller_str2d();
			}
		}
	}
}

int controller_init(void)
{
	rt_thread_t thread = RT_NULL; 

	thread = rt_thread_create("pid_ctrl", controller_entry, RT_NULL, 1024, 15, 10);

	if(thread == RT_NULL)
    {
        return RT_ERROR;
    }
    rt_thread_startup(thread);

    return RT_EOK;
}
INIT_APP_EXPORT(controller_init);

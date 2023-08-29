#ifndef PROJECT_HEADER_H
#define PROJECT_HEADER_H

typedef unsigned tid_t; /*тип идентификатора задачи*/
#define numberOfTasks 10

#ifdef __cplusplus
extern "C" {
#endif
	void task(tid_t taskId);

	void requestTask(tid_t taskId);

	void runTaskManager(tid_t taskId);

	

#ifdef __cplusplus
}
#endif

#endif
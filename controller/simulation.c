#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../import/simulation.h"
#include "../import/rngs.h"
#include "../import/rvgs.h"
#include "../import/calqueue.h"
#include "../import/config.h"

#define LONG_MAX 2147483647
#define DAY_TO_MIN 1440
#define MIN_TO_HR 60
static long clock;      // Simulation clock, keeps track of the current time
static long start_clock;
state* st;
config* conf;
calqueue* eventList;
outputStats *stats;
calqueue *queue;

/* schedule new event. 
/* @param: type of event
/* @param: priority of job
/* @param: index server that execute the service
*/
void scheduleEvent(eventType type, priority prior, int idxServer){

    SelectStream(type);
    event* e = malloc(sizeof(event));
    if(!e){
        perror("New event creation error: Memory error");
        return;
    }
    *e = (const event) {0};
    double r;
    e->idxServer = idxServer;
    e->type = type;
    e->prior = prior;
    switch (type){
    case arrival:
        // Set priority
        SelectStream(type+7);
        r = Random();
        if(r < conf->pr1_p) e->prior = 1;
        else if(r < conf->pr1_p + conf->pr2_p)  e->prior = 2;
        else if(r < conf->pr1_p + conf->pr2_p + conf->pr3_p) e->prior = 3;
        else  e->prior = 4;
        // Set time
        SelectStream(type);
        e->time = clock + Exponential(1/conf->arriv_mu * DAY_TO_MIN);
        break;
    
    case serv_dev:
        e->time = clock + Exponential(1/conf->dev_mu * DAY_TO_MIN);
        break;
    
    case serv_test:
        e->time = clock + Exponential(1/conf->test_mu * DAY_TO_MIN);
        break;

    case serv_prod:
        e->time = clock + Exponential(1/conf->prod_mu * DAY_TO_MIN);
        break;

    case serv_delayProd:
        e->time = clock + Exponential(1/conf->delayProd_mu * DAY_TO_MIN);        
        break;

    case serv_rollback:
        e->time = clock + Exponential(1/conf->rollback_mu * DAY_TO_MIN);
        break;

    case serv_fastDev:
        e->time = clock + Exponential(1/conf->fastDev_mu * DAY_TO_MIN);
        break;   
    }
    // Add event
    calqueue_put(eventList, e->time, e);
}

/* update statistics: Is called every time a job completed its execution
/* @param: job completed
*/
void updStats(jobStr* job){
    int prior = job->prior-1;
    double jobWait = 0;
    double jobService = 0;

    stats->jobs_output++;
    stats->pr_jobs_output[prior]++;

    for (int i = 0; i < 6; i++){
        if(job->passed[i]>0){
            stats->jobs_centerWgt[i]++;
            stats->avgWaitCenterWgt[i] += ((float)job->wait[i] - stats->avgWaitCenterWgt[i]) / ((float)stats->jobs_centerWgt[i]);
            stats->avgServiceCenterWgt[i] += ((float)job->service[i] - stats->avgServiceCenterWgt[i]) / ((float)stats->jobs_centerWgt[i]);
            stats->avgResponseCenterWgt[i] = stats->avgWaitCenterWgt[i] + stats->avgServiceCenterWgt[i];
            jobWait += (float)job->wait[i];
            jobService += (float)job->service[i];
        }    
        for (int j = 0; j <job->passed[i]; j++){
            stats->jobs_center[i]++;
            stats->avgWaitCenter[i] += ((float)job->wait[i]/((float)job->passed[i]) - stats->avgWaitCenter[i]) / ((float)stats->jobs_center[i]);
            stats->avgServiceCenter[i] += ((float)job->service[i]/((float)job->passed[i]) - stats->avgServiceCenter[i]) / ((float)stats->jobs_center[i]);
            stats->avgResponseCenter[i] = stats->avgWaitCenter[i] + stats->avgServiceCenter[i];
        }

    }
    stats->pr_avgWait[prior] += ((float)jobWait - stats->pr_avgWait[prior]) / ((float)stats->pr_jobs_output[prior]);
    stats->pr_avgService[prior] += ((float)jobService - stats->pr_avgService[prior]) / ((float)stats->pr_jobs_output[prior]);
    stats->pr_avgResponse[prior] = stats->pr_avgWait[prior] + stats->pr_avgService[prior];

    free(job);
}

/* 
/* last update statistics: Is called at the end of the simulation
*/
void finalUpdStats(){
    double lambda[6];
    for(int i = 0; i < 6; i++){
        stats->avgWait += stats->avgWaitCenter[i];
        stats->avgService += stats->avgServiceCenter[i];
        stats->avgResponse += stats->avgResponseCenter[i];

        lambda[i] = (float)stats->jobs_center[i] / (clock - start_clock);
        stats->w_centers[i] = lambda[i] * stats->avgWaitCenter[i];
        stats->x_centers[i] = lambda[i] * stats->avgServiceCenter[i];
        stats->l_centers[i] = lambda[i] * stats->avgResponseCenter[i];
    }
    if(stats->x_centers[0] > 1 && conf->dev_n == 1) stats->x_centers[0] = 1;
}

/* 
/*  find server free in multiserver center
*/
int findFreeServer(multiServer serv){
    int i = 0;
    while(i < serv.server_n){
        if(serv.servers[i] == NULL) return i;
        i++;
    }
    return -1;
}

/* 
/* find job in queues of multiserver
*/
int findJobQueue(multiServer serv){
    int i = 0;
    while(i < serv.queue_n){
        if(serv.queues[i] != NULL) return i;
        i++;
    }
    return -1;
}

/* 
/* add job in the queue
/* @param: queue
/* @param: job
*/
void addJobQueue(jobStr** queue, jobStr* job){   /// (?) forse da correggere il puntatore
    jobStr *curr;
    if(*queue == NULL){
        // empty queue
        (*queue) = job;
        return;
    }

    curr = *queue;
    while(curr->next != NULL){
        // navigate the queue
        curr = curr->next;
    }
    curr->next = job;
}

/* 
/* insert job in multiserver
/* @param: job
/* @param: multiserver
/* @param: type of job
*/
void put_jobMultiSrv(jobStr* job, multiServer* srv, eventType type){
    job->arrive_center = clock;
    job->next = NULL;

    job->passed[type-1]++;
    int idx = findFreeServer(*srv);
    if(idx != -1){
        // Free server
        srv->servers[idx] = job;
        scheduleEvent(type, job->prior, idx);
    }
    else{
        // All server are busy: Insert to queue 
        if(type != serv_prod) addJobQueue(&(srv->queues[job->prior-2]), job);
        else addJobQueue(&(srv->queues[job->prior-1]), job);
    }
}

/* 
/* insert job in single queue
/* @param: job
/* @param: single server center
/* @param: type of job
*/
void put_jobSingleSrv(jobStr* job, singleServer* srv, eventType type){
    job->arrive_center = clock;
    job->next = NULL;
    
    job->passed[type-1]++;
    if(srv->server == NULL){
        // Free server
        srv->server = job;
        scheduleEvent(type, job->prior, 0);
    }
    else{
        // All server are busy: Insert to queue 
        addJobQueue(&(srv->queue), job);
    }
}

/* 
/* free the server and insert a new job from the queue
/* @param: multiserver
/* @param: type of center
/* @param: index server
*/
void free_jobMultiSrv(multiServer* srv, eventType type, int idxSrv){
    srv->servers[idxSrv] = NULL;
    int idx = findJobQueue(*srv);
    if(idx != -1){
        // Find a queue with job: Prioriry order of selection
        jobStr* job = srv->queues[idx];
        srv->queues[idx] = job->next;
        job->wait[type-1] += clock - job->arrive_center;
        job->arrive_center = clock;
        job->next = NULL;

        srv->servers[idxSrv] = job;
        scheduleEvent(type, job->prior, idxSrv);
    }
}

/* 
/* free the server and insert a new job from the queue
/* @param: single server
/* @param: type of center
*/
void free_jobSingleSrv(singleServer* srv, eventType type){
    srv->server = NULL;
    jobStr* job = srv->queue;
    if(job != NULL){
        // Find a job in queue
        job = srv->queue;
        srv->queue = job->next;
        job->wait[type-1] += (clock - job->arrive_center);
        job->arrive_center = clock;
        job->next = NULL;
        srv->server = job;
        scheduleEvent(type, job->prior, 0);
    }
}

/* 
/* execute arrival
/* @param: event 
*/
void exec_arrival(event* e){
    int idx;
    // update stats
    stats->jobs_input++;

    // create job
    jobStr* job = (jobStr*) malloc(sizeof(jobStr));
    *job = (const jobStr) {0};

    if(!job){
        perror("New job creation error: Memory error");
        return;
    }
    job->arrive = clock;
    job->prior = e->prior;

    // update input service center
    switch(e->prior){
        // Very high priority arrive
        case 1:
            put_jobSingleSrv(job, &(st->fastDev), serv_fastDev);
            break;
        default:
            put_jobMultiSrv(job, &(st->dev), serv_dev);
            break;
    }
    scheduleEvent(arrival, -1, -1);
}

/* 
/* execute service: 6 type of execution
/* @param: event 
*/
void exec_serv(event* e){
    int idxSrv = e->idxServer;
    eventType type = e->type;

    jobStr *job = NULL;
    double r;
    SelectStream(type+7);

    if(type == serv_dev){
        job = st->dev.servers[idxSrv];
        job->service[type-1] += clock - job->arrive_center;
        free_jobMultiSrv(&(st->dev), serv_dev, idxSrv);
        put_jobMultiSrv(job, &(st->test), serv_test);
    }

    else if(type == serv_test){
        job = st->test.servers[idxSrv];
        job->service[type-1] += clock - job->arrive_center;
        
        free_jobMultiSrv(&(st->test), serv_test, idxSrv);

        r = Random();
        if(r < conf->testToDev_p)   put_jobMultiSrv(job, &(st->dev), serv_dev);
        else if(r < conf->testToDev_p + conf->delay_p * (1 - conf->testToDev_p))  put_jobSingleSrv(job, &(st->delayProd), serv_delayProd);
        else    put_jobMultiSrv(job, &(st->prod), serv_prod);
    }

    else if(type == serv_prod){
        job = st->prod.servers[idxSrv];
        job->service[type-1] += clock - job->arrive_center;

        free_jobMultiSrv(&(st->prod), serv_prod, idxSrv);

        r = Random();

        if(job->prior == 1) {
            updStats(job);
            return;
        }

        if(r < conf->prodToDev_p)  put_jobMultiSrv(job, &(st->dev), serv_dev);
        else if(r < conf->prodToDev_p + conf->prodToTest_p)   put_jobMultiSrv(job, &(st->test), serv_test);
        else if (r < conf->prodToDev_p + conf->prodToTest_p + conf->rollback_p)  put_jobSingleSrv(job, &(st->rollback), serv_rollback);
        else updStats(job);
    }

    else if(type == serv_delayProd){
        job = st->delayProd.server;
        job->service[type-1] += clock - job->arrive_center;
        free_jobSingleSrv(&(st->delayProd), serv_delayProd);
        put_jobMultiSrv(job, &(st->prod), serv_prod);
    }

    else if(type == serv_rollback){
        job = st->rollback.server;
        job->service[type-1] += clock - job->arrive_center;
        free_jobSingleSrv(&(st->rollback), serv_rollback);
        put_jobMultiSrv(job, &(st->dev), serv_dev);
    }

    else if(type == serv_fastDev){
        job = st->fastDev.server;
        job->service[type-1] += clock - job->arrive_center;
        free_jobSingleSrv(&(st->fastDev), serv_fastDev);
        put_jobMultiSrv(job, &(st->prod), serv_prod);
    }
}



// SIMULATION ----------------------------------------------------------------------------------------------------------

/*
 * Simulate the given configuration
 *
 * conf: pointer to a configuration structure
 * st: pointer to a state structure
 * stats_in: pointer to a result structure
 * eventList: pointer to the event list
 * test: starting clock
 *
 */

void simulate(config *conf_in, state *st_in, calqueue *eventList, outputStats *stats_in, long *clk){
    // 1) INITIALIZE
    // 1.1) Clock
    clock = *clk;
    start_clock = *clk;

    // i.2) Other variables
    int i, j;
    conf = conf_in;
    st = st_in;
    stats = stats_in;
    eventList = eventList;
    event* e;

    // START SIMULATION
    if(clock == 0){
        // Plant seed
        PlantSeeds(conf->seed); 
        scheduleEvent(arrival, -1, -1);
    } 	

    while(clock < conf->close_the_door && stats->jobs_output < conf->totalJobs){ 
        // 2) GET EVENT 
	 	e = calqueue_get(eventList);

        // 3) ADVANCE
        clock = e->time;

        // 4) PROCESS AND SCHEDULE
        if(e->type == arrival) exec_arrival(e);
        else exec_serv(e);

        // 5) REMOVE EVENT
	    free(e);
    }

    // 6) GENERATE STATISTICS
    finalUpdStats();

    *clk = clock;
}

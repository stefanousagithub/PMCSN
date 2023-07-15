#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../import/simulation.h"
#include "../import/rngs.h"
#include "../import/rvgs.h"
#include "../import/calqueue.h"
#include "../import/config.h"
#include "../import/rvms.h"

#define LONG_MAX 2147483647
#define DAY_TO_MIN 1440
#define MIN_TO_HR 60
#define LOC 0.95
state *st;
config *conf;
outputStats *stats;
calqueue *eventList;
static long clock;
static long start_clock;

/*
/* set configuration
*/
void setConf()
{
    conf->seed = SEED; // default seed
    conf->close_the_door = CLOSE_THE_DOOR;
    conf->totalJobs = TOTAL_JOBS;

    conf->dev_n = DEV_N;
    conf->test_n = TEST_N;
    conf->prod_n = PROD_N;

    conf->arriv_mu = ARRIV_MU; // arrival in default flow [job/min]
    conf->dev_mu = DEV_MU;
    conf->test_mu = TEST_MU;
    conf->prod_mu = PROD_MU;
    conf->delayProd_mu = DELAYPROD_MU;
    conf->rollback_mu = ROLLBACK_MU;
    conf->fastDev_mu = FASTDEV_MU;

    conf->pr1_p = PR1_P;
    conf->pr2_p = PR2_P;
    conf->pr3_p = PR3_P;
    conf->pr4_p = PR4_P;
    conf->delay_p = DELAY_P;
    conf->testToDev_p = TESTTODEV_P;
    conf->prodToTest_p = PRODTOTEST_P;
    conf->prodToDev_p = PRODTODEV_P;
    conf->rollback_p = ROLLBACK_P;
}

/*
/* free state
*/
void clear(){
    // clear jobs
    jobStr* job;
    for(int i = 0; i <conf->dev_n;i++){
        if(st->dev.servers[i] != NULL){
            free(st->dev.servers[i]);
        }
    }
    for(int i = 0; i < 3;i++){
        while(st->dev.queues[i] != NULL){
            job = st->dev.queues[i];
            st->dev.queues[i] = job->next;
            free(job);
        }
    }

    for(int i = 0; i <conf->test_n;i++){
        if(st->test.servers[i] != NULL){
            free(st->test.servers[i]);
        }
    }
    for(int i = 0; i < 3;i++){
        while(st->test.queues[i] != NULL){
            job = st->test.queues[i];
            st->test.queues[i] = job->next;
            free(job);
        }
    }
    for(int i = 0; i <conf->prod_n;i++){
        if(st->prod.servers[i] != NULL){
            free(st->prod.servers[i]);
        }
    }
    for(int i = 0; i < 4;i++){
        while(st->prod.queues[i] != NULL){
            job = st->prod.queues[i];
            st->prod.queues[i] = job->next;
            free(job);
        }
    }

    free(st->dev.queues);
    free(st->dev.servers);
    free(st->test.queues);
    free(st->test.servers);
    free(st->prod.queues);
    free(st->prod.servers);
    free(st);
}

/*
/* initialize multiservers queues and servers
*/
void initMultiServer(multiServer *dev, multiServer *test, multiServer *prod)
{
    int i = 0;
    dev->queues = (jobStr **)malloc(sizeof(jobStr *) * 3);
    dev->servers = (jobStr **)malloc(sizeof(jobStr *) * conf->dev_n);
    if (!dev->queues || !dev->servers)
    {
        perror("New state creation error");
        return;
    }
    dev->queues[0] = NULL;
    dev->queues[1] = NULL;
    dev->queues[2] = NULL;
    for (int j = 0; j < conf->dev_n; j++)
    {
        dev->servers[j] = NULL;
    }

    test->queues = (jobStr **)malloc(sizeof(jobStr *) * 3);
    test->servers = (jobStr **)malloc(sizeof(jobStr *) * conf->test_n);

    if (!test->queues || !test->servers)
    {
        perror("New state creation error");
        return;
    }
    test->queues[0] = NULL;
    test->queues[1] = NULL;
    test->queues[2] = NULL;
    for (int j = 0; j < conf->test_n; j++)
    {
        test->servers[j] = NULL;
    }

    prod->queues = (jobStr **)malloc(sizeof(jobStr *) * 4);
    prod->servers = (jobStr **)malloc(sizeof(jobStr *) * conf->prod_n);
    if (!prod->queues || !prod->servers)
    {
        perror("New state creation error");
        return;
    }
    prod->queues[0] = NULL;
    prod->queues[1] = NULL;
    prod->queues[2] = NULL;
    prod->queues[3] = NULL;

    for (int j = 0; j < conf->prod_n; j++)
    {
        prod->servers[j] = NULL;
    }
}

/*
/* set initial state
*/
void setState(){
    // Initialize state
    st = (state *)malloc(sizeof(state));
    if (!st)
    {
        perror("New state creation error");
        return;
    }
    *st = (const state){0};
    multiServer dev = {conf->dev_mu, conf->dev_n, 3, NULL, NULL, serv_dev};
    multiServer test = {conf->test_mu, conf->test_n, 3, NULL, NULL, serv_test};
    multiServer prod = {conf->prod_mu, conf->prod_n, 4, NULL, NULL, serv_prod};
    initMultiServer(&dev, &test, &prod);
    singleServer delayProd = {conf->delayProd_mu, NULL, NULL, serv_delayProd};
    singleServer rollback = {conf->rollback_mu, NULL, NULL, serv_rollback};
    singleServer fastDev = {conf->fastDev_mu, NULL, NULL, serv_fastDev};
    st->dev = dev;
    st->test = test;
    st->prod = prod;
    st->delayProd = delayProd;
    st->rollback = rollback;
    st->fastDev = fastDev;
}


/*
/* set statistics of a sigle simulation
*/
void printStats()
{
    printf("\n############################################################");
    printf("\n-> Config\n");
    printf("\n# Seed: %ld", conf->seed);
    printf("\n# Close the door: %ld (days)", conf->close_the_door / DAY_TO_MIN);
    printf("\n# Total jobs: %ld\n", conf->totalJobs);
    printf("\n# Develop servers: %d", conf->dev_n);
    printf("\n# Test servers: %d", conf->test_n);
    printf("\n# Production servers: %d\n", conf->prod_n);
    printf("\n# Arrival rate (Lambda): %.3f", conf->arriv_mu);
    printf("\n# develop rate: %.3f", conf->dev_mu);
    printf("\n# test rate: %.3f", conf->test_mu);
    printf("\n# production rate: %.3f", conf->prod_mu);
    printf("\n# delay production rate: %.3f", conf->delayProd_mu);
    printf("\n# rollback rate: %.3f", conf->rollback_mu);
    printf("\n# Fast dev and test rate: %.3f\n", conf->fastDev_mu);
    printf("\n# Probab. priority 1: %.3f", conf->pr1_p);
    printf("\n# Probab. priority 2: %.3f", conf->pr2_p);
    printf("\n# Probab. priority 3: %.3f", conf->pr3_p);
    printf("\n# Probab. priority 4: %.3f", conf->pr4_p);
    printf("\n# Probab. delay: %.3f", conf->delay_p);
    printf("\n# Probab. test to dev feedback: %.3f", conf->testToDev_p);
    printf("\n# Probab. prod to test feedback: %.3f", conf->prodToTest_p);
    printf("\n# Probab. prod to dev feedback: %.3f", conf->prodToDev_p);
    printf("\n# Probab. rollback: %.3f\n", conf->rollback_p);

    printf("\n------------------------------------------------------------");
    printf("\n--------------------  SIMULATION  --------------------------");
    printf("\n------------------------------------------------------------\n");

    printf("\nTotal jobs: %ld (output: %ld) [pr1:%ld, pr2:%ld,pr3:%ld,pr4:%ld]\n", stats->jobs_input,
           stats->jobs_output, stats->pr_jobs_output[0], stats->pr_jobs_output[1], stats->pr_jobs_output[2], stats->pr_jobs_output[3]);
    printf("\nAvg wait time: %.2fh", stats->avgWait / MIN_TO_HR);
    printf("\nAvg service time: %.2fh", stats->avgService / MIN_TO_HR);
    printf("\nAvg response time: %.2fh", stats->avgResponse / MIN_TO_HR);
    printf("\n\n-> Priorities");

    for (int i = 0; i < 4; i++)
    {
        printf("\n[Prior %d] job: %ld, wait: %.2fh, service: %.2fh, response: %.2fh",
               i + 1, stats->pr_jobs_output[i], stats->pr_avgWait[i] / MIN_TO_HR, stats->pr_avgService[i] / MIN_TO_HR, stats->pr_avgResponse[i] / MIN_TO_HR);
    }

    printf("\n\n-> Centers");
    for (int i = 0; i < 6; i++)
    {
        printf("\n[Center %d] job: %ld, wait: %.2fh, service: %.2fh, response: %.2fh",
               i + 1, stats->jobs_center[i], stats->avgWaitCenter[i] / MIN_TO_HR, stats->avgServiceCenter[i] / MIN_TO_HR, stats->avgResponseCenter[i] / MIN_TO_HR);
    }

    printf("\n\n-> Centers wighted: The feedback is refered as the old job");
    for (int i = 0; i < 6; i++)
    {
        printf("\n[Center %d] job: %ld, wait: %.2fh, service: %.2fh, response: %.2fh",
               i + 1, stats->jobs_centerWgt[i], stats->avgWaitCenterWgt[i] / MIN_TO_HR, stats->avgServiceCenterWgt[i] / MIN_TO_HR, stats->avgResponseCenterWgt[i] / MIN_TO_HR);
    }

    printf("\n\n-> Little Law");
    for (int i = 0; i < 6; i++){
        printf("\n[Center %d] job queue: %.3f, service: %.3f, center: %.3f",
               i + 1, stats->w_centers[i], stats->x_centers[i], stats->l_centers[i]);
    }
    printf("\n\n# Time in hours");
    printf("\n\n");
    fflush(stdout);
}

/*
/* reset simulation: clock, event list, state, statistics
*/
void resetSimul(){
    clock = 0;
    config c = (const config){0};
    outputStats s = (const outputStats){0};
    conf = &c;
    stats = &s;
    setConf();
    setState();

    // 1.4) List Event
	eventList = malloc(sizeof(calqueue));
    if(!eventList){
        perror("New event list creation error");
        return;
    }
    *eventList = (const calqueue) {0};
	calqueue_init(eventList);
}

/*
/* function called by analyzer function in python
/* calculate batch means and 95% confidence for a center
*/
void verification(long b, long k, int center){
    /* counts data points */
    long n = 0;
    double sum[7] = {0};
    double mean[7] = {0};
    double data[7];
    double stdev[7];
    double u[7], t[7], w[7];
    double diff[7];
    clock = 0;

    resetSimul();

    conf->totalJobs = b;
    /* use Welford's one-pass method */
    /* to calculate the sample mean */
    /* and standard deviation */
    for (int i = 0; i < k; i++){
        *stats = (const outputStats){0};
        start_clock = clock;
        simulate(conf, st, eventList, stats, &clock);
        n++;
        for(int j = 0; j < 7; j++){
            switch (j){
                case 0:
                    data[j] = stats->avgWaitCenter[center] / MIN_TO_HR;
                    break;
                 case 1:
                    data[j] = stats->avgServiceCenter[center] / MIN_TO_HR;
                    break;
                case 2:
                    data[j] = stats->avgResponseCenter[center]/ MIN_TO_HR;
                    break;
                case 3:
                    data[j] = stats->w_centers[center];
                    break;
                case 4:
                    data[j] = stats->x_centers[center];
                    break;
                case 5:
                    data[j] = stats->l_centers[center];
                    break;
                case 6:
                    data[j] = (float)stats->jobs_center[center] / (float)(clock - start_clock) * DAY_TO_MIN;
                    break;
            }

            diff[j] = data[j] - mean[j];
            sum[j] += diff[j] * diff[j] * (n - 1.0) / n;
            mean[j] += diff[j] / n;
        }
    }
    for(int j = 0; j < 7; j++){
        stdev[j] = sqrt(sum[j] / n);
    }

    if (n > 1) {
        printf("\nBatch means (b = %ld, k = %ld)", b, k);
        printf(" with %d%% confidence\n", (int) (100.0 * LOC + 0.5));
        printf("\nCenter %d\n", center);

        for(int j = 0; j < 7; j++){
            u[j] = 1.0 - 0.5 * (1.0 - LOC);
            t[j] = idfStudent(n - 1, u[j]);
            w[j] = t[j] * stdev[j] / sqrt(n - 1);
            switch (j){
                case 0:
                    printf("E[Tq]:%10.4fh +/- %6.4fh\n", mean[j], w[j]); 
                    break;
                case 1:
                    printf("E[s]:%10.4fh +/- %6.4fh\n", mean[j], w[j]); 
                    break;
                case 2:
                    printf("E[Ts]:%10.4fh +/- %6.4fh\n", mean[j], w[j]); 
                    break;
                case 3:
                    printf("E[w]:%10.4f +/- %6.4f\n", mean[j], w[j]); 
                    break;
                case 4:
                    printf("E[x]:%10.4f +/- %6.4f\n", mean[j], w[j]); 
                    break;
                case 5:                
                    printf("E[l]:%10.4f +/- %6.4f\n", mean[j], w[j]); 
                    break;
                case 6:                
                    printf("Lambda':%10.4f +/- %6.4f\n", mean[j], w[j]); 
                    break;
            }
        }
    }
    else printf("ERROR verification - insufficient data\n");
}

/*
/* struct for verification function
*/
typedef struct IOresults{
    double respCenter1;
    double respCenter2;
    double respCenter3;
    double respCenter4;
    double respCenter5;
    double respCenter6;

    double utilCenter1;
    double utilCenter2;
    double utilCenter3;
    double utilCenter4;
    double utilCenter5;
    double utilCenter6;

    double respJobPr1;
    double respJobPr2;
    double respJobPr3;
    double respJobPr4;
} IOresults;


IOresults* analyse(int setting){
/* Function called by python analizer: 
/* It returns some import statistics 
/* using a steady-state analisys with 300 000 jobs 
/* 
/* Settings influence the type of analisys:
/* - 1 Increase Lambda
/* - 2 Decrease services
/* - 3 delete feedbacks
/* - 4 Increase number of servers
*/ 

    IOresults* io = (IOresults*) malloc(sizeof(IOresults)); 
    if(!io){
        perror("New event list creation error");
        return NULL;
    }
    
    resetSimul();
    conf->totalJobs = 100000;
    switch(setting){
        case 1:
            conf->arriv_mu = 4.1;
            break;
        case 2:
            conf->dev_mu += 1;
            conf->test_mu += 1;
            conf->prod_mu += 1;
            conf->delayProd_mu += 1;
            conf->rollback_mu += 1;
            conf->fastDev_mu += 1;
            break;
        case 3:
            conf->testToDev_p = 0;
            conf->prodToDev_p = 0;
            conf->prodToTest_p = 0;
            conf->rollback_p = 0;
            break;
        case 4:
            conf->dev_n += 1;
            conf->test_n += 1;
            conf->prod_n += 1;
            clear(); // clear state
            setState(); // set new state
            break;
    }

    simulate(conf, st, eventList, stats, &clock);
    io->respCenter1 = stats->avgResponseCenter[0]/MIN_TO_HR;
    io->respCenter2 = stats->avgResponseCenter[1]/MIN_TO_HR;
    io->respCenter3 = stats->avgResponseCenter[2]/MIN_TO_HR;
    io->respCenter4 = stats->avgResponseCenter[3]/MIN_TO_HR;
    io->respCenter5 = stats->avgResponseCenter[4]/MIN_TO_HR;
    io->respCenter6 = stats->avgResponseCenter[5]/MIN_TO_HR;
    io->utilCenter1 = stats->x_centers[0];
    io->utilCenter2 = stats->x_centers[1];
    io->utilCenter3 = stats->x_centers[2];
    io->utilCenter4 = stats->x_centers[3];
    io->utilCenter5 = stats->x_centers[4];
    io->utilCenter6 = stats->x_centers[5];
    io->respJobPr1 = stats->pr_avgResponse[0]/MIN_TO_HR;
    io->respJobPr2 = stats->pr_avgResponse[1]/MIN_TO_HR;
    io->respJobPr3 = stats->pr_avgResponse[2]/MIN_TO_HR;
    io->respJobPr4 = stats->pr_avgResponse[3]/MIN_TO_HR;

    // free space
    clear();
    free(eventList);
    return io;

}


void clearAnalyse(IOresults* io){
    free(io);
}

void clearFinHoriz(double* io){
    free(io);
}

double* finiteHorizon(long seed){
/* Function called by python analizer: 
/* It returns response time of develop center 
/* using a transient analisys with 500 jobs. 
/* 
*/ 
    int i = 0;
    double *avgResponse = (double*) malloc(sizeof(double)*16);
    if(!avgResponse){
        perror("New event list creation error");
        return NULL;
    }
    for(i = 0; i < 16; i++){
        resetSimul();
        conf->seed = seed;
        conf->close_the_door = 4*(pow(2,i))*DAY_TO_MIN;
        simulate(conf, st, eventList, stats, &clock);
        avgResponse[i] = stats->avgResponse/MIN_TO_HR;
        
        // free space        
        clear();
        free(eventList);
    }
    return avgResponse;
}

int main() {
    // NORMAL SINGLE SIMULATION
    resetSimul();
    conf->totalJobs = 400000;
    simulate(conf, st, eventList, stats, &clock);
    printStats();
    return 0;
}


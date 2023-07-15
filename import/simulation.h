// Struct which keeps all the configuration parameters
#include "calqueue.h"
typedef struct config {

    long seed;              // Initial seed
    long close_the_door;    // Simulation end time
    long totalJobs;         // Total number jobs

    int dev_n;		     // number servers develop							
    int test_n;	     // number servers test
    int prod_n;	     // number servers production

    double arriv_mu;          // arrival flow [job/day]
    double dev_mu;	       // Service devep server
    double test_mu;		// Service test server
    double prod_mu;		// Service production server
    double delayProd_mu;	// Service delay production server
    double rollback_mu;	// Service rollback server
    double fastDev_mu;		// Service fast development and test server

    double pr1_p;		// probability job priority "very high"
    double pr2_p;		// probability job priority "high"
    double pr3_p;		// probability job priority "medium"
    double pr4_p;		// probability job priority "low"
    double delay_p;		// probability delayed work
    double testToDev_p;	// probability feedback test to development
    double prodToTest_p;	// probability feedback production to test
    double prodToDev_p;	// probability feedback production to development
    double rollback_p;		// probability feedback production to development with rollback
} config;

typedef enum priority {
    very_high,
    high,
    medium,
    low
} priority;

typedef enum eventType {
    arrival = 0,		// arrive event
    serv_dev = 1,		// service in development	
    serv_test = 2,		// service in test
    serv_prod = 3,		// service in production
    serv_delayProd = 4,	// service in delayed production
    serv_rollback = 5,		// service in rollback
    serv_fastDev = 6,		// service in fast development and test
} eventType;

typedef struct jobStr{
    long arrive;		// arrive job in the system
    priority prior;		// priority of the job

    long arrive_center;	// dynamic value: Record arrive in current queue or server
    struct jobStr* next;	// dynamic value: Record the next job in the queue

    long wait[6];      // wait time for  dev, test, prod, delayProd, rollback, fastDev
    long service[6];   // service time for dev, test, prod, delayProd, rollback, fastDev
    int passed[6];	// record what centers the job passed on

} jobStr;



typedef struct singleServer{
    double service;		// service time
    jobStr* queue;		// Linked list to jobs
    jobStr* server;		// Job pointer
    int stream;		// stream for pseudo-random function
} singleServer;

typedef struct multiServer{
    double service;		// service time
    
    int server_n;		// number of servers
    int queue_n;		// number of queues
    
    jobStr** queues;		// array of linked list to jobs
    jobStr** servers;		// array of job pointer
    int stream;		// stream for pseudo-random function

} multiServer;

typedef struct state{
    multiServer dev;		// development center
    multiServer test;		// test center
    multiServer prod;		// production center
    singleServer delayProd;	// delay production center
    singleServer rollback;	// rollback center
    singleServer fastDev;	// fast development and test center
} state;

// 0.005 0.30 0.60 0.005
typedef struct event{
    long time;			// time
    eventType type;		// Arrive or service (6 type of service)
    priority prior;		// priority of the job
    int idxServer;		// Server executed (set to 0 if single server center)
} event;

typedef struct outputStats{
    long jobs_input;			// jobs input system
    long jobs_output;			// jobs output system
    long pr_jobs_output[4];		// jobs categorized in priority output system
    long jobs_center[6];
    long jobs_centerWgt[6];


    double avgWait;			// Avg statistics of system
    double avgService;
    double avgResponse;

    double avgWaitCenter[6];		// Avg statics of centers
    double avgServiceCenter[6];
    double avgResponseCenter[6];

    double avgWaitCenterWgt[6];		// Avg statics weighted for job: Feedback are not considerated as new jobs
    double avgServiceCenterWgt[6];
    double avgResponseCenterWgt[6];

    double w_centers[6];		// avg number in queue, in servers and in the centers
    double x_centers[6];
    double l_centers[6];

    double pr_avgWait[4];		// avg statistics categorized in priority
    double pr_avgService[4];
    double pr_avgResponse[4];

} outputStats;

void simulate(config *conf_in, state *st_in, calqueue *eventList, outputStats *stats_in, long*clk);

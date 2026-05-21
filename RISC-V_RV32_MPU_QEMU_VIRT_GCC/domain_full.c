/* Standard includes. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

/* Priorities at which the tasks are created. */
//#define mainQUEUE_RECEIVE_TASK_PRIORITY    (( tskIDLE_PRIORITY + 2 ) | portPRIVILEGE_BIT )
//#define mainQUEUE_SEND_TASK_PRIORITY       (( tskIDLE_PRIORITY + 1 ) | portPRIVILEGE_BIT )
#define mainPRIVILEGED_PRIORITY    (( tskIDLE_PRIORITY + 1 ))
#define mainUNPRIVILEGED_PRIORITY       (( tskIDLE_PRIORITY + 2 ))

/* The rate at which data is sent to the queue.  The times are converted from
 * milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS         pdMS_TO_TICKS( 10UL )
#define mainTIMER_SEND_FREQUENCY_MS        pdMS_TO_TICKS( 2000UL )

/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH                   ( 2 )

/* The values sent to the queue receive task from the queue send task and the
 * queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TASK           ( 100UL )
#define mainVALUE_SENT_FROM_TIMER          ( 200UL )

#define NUM_DOMAINS (4)

/*-----------------------------------------------------------*/

/*
 * Domain privileged creates an domainUnPrivileged task within the domain, which prints the domain tick whenever the xTick increments
 */
static void domainPrivileged(void* pvParameters);
static void domainUnPrivileged(void* pvParameters);

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

extern char _text[];
extern char _etext[];
extern char _rodata[];
extern char _erodata[];
extern char _data[];
extern char _edata[];
extern char _bss[];
extern char _ebss[];
extern char _privileged_data[];
extern char _eprivileged_data[];

/*-----------------------------------------------------------*/

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void domain_full( void )
{
    __asm__ volatile( "csrs mcounteren, %0" :: "r"(5) );
    __asm__ volatile( "csrs scounteren, %0" :: "r"(5) );
    printf ( "MPU blinky demo start\n" );
    printf ( ".text   = [0x%x - 0x%x]\n", &_text, &_etext );
    printf ( ".rodata = [0x%x - 0x%x]\n", &_rodata, &_erodata );
    printf ( ".data   = [0x%x - 0x%x]\n", &_data, &_edata );
    printf ( ".bss    = [0x%x - 0x%x]\n", &_bss , &_ebss );
    printf ( ".privileged_data = [0x%x - 0x%x]\n", &_privileged_data, &_eprivileged_data );

    static StackType_t xPrivilegedStack[ NUM_DOMAINS ][ configMINIMAL_STACK_SIZE*2 ] __attribute__((aligned( configMINIMAL_STACK_SIZE * 2 * sizeof(StackType_t) )));

    for (size_t i = 0; i < NUM_DOMAINS; i++) {
        DomainParameters_t DomainParameters = {
            .ulSliceIndex = i*2 + 1,
            .ulSliceLength = 2,
        };
        TaskParameters_t xDomainTaskParameters =
        {
        .pvTaskCode     = domainPrivileged,
            .pcName         = "Rx",
            .usStackDepth   = configMINIMAL_STACK_SIZE * 2,
            .pvParameters   = (void* ) (i + 1),
            .uxPriority     = mainPRIVILEGED_PRIORITY | portPRIVILEGE_BIT,
            .puxStackBuffer = xPrivilegedStack[i],
            .xRegions       =
            {
                { &_text, _erodata - _text, portMPU_REGION_READ | portMPU_REGION_EXECUTE },
                { &_data, _ebss - _data, portMPU_REGION_READ | portMPU_REGION_WRITE },
                /* NS16550 */
                { (void *)0x10000000UL, 0x1000, portMPU_REGION_READ | portMPU_REGION_WRITE },
                { 0, 0, 0 },
                { 0, 0, 0 },
                { 0, 0, 0 },
                { 0, 0, 0 },
                /* *MUST* reserve one entry for stack */
                { 0, 0, 0 },
            }
            #if ( configENABLE_DOMAINS == 1 )
            ,.pxDomainParameters  = &DomainParameters,
            #endif
        };

        TaskHandle_t xDomTask;

        BaseType_t ret = pdFAIL;
        while (ret != pdPASS) {
            ret = xTaskCreateRestricted( &( xDomainTaskParameters ), &xDomTask );
        }
    }

    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

static void domainPrivileged(void* pvParameters) {
    uint32_t dom_num = (uint32_t) pvParameters;
    TickType_t domain_tick = xTaskGetDomainTick();
    printf ( "Privileged task: %d, DomainTick: %d\n", dom_num, domain_tick);
    static StackType_t xPrivilegedUnStack[ NUM_DOMAINS ][ configMINIMAL_STACK_SIZE ] __attribute__((aligned( configMINIMAL_STACK_SIZE * sizeof(StackType_t) )));

    TaskParameters_t xDomainTaskParameters =
    {
        .pvTaskCode     = domainUnPrivileged,
        .pcName         = "Rx",
        .usStackDepth   = configMINIMAL_STACK_SIZE,
        .pvParameters   = (void* ) dom_num,
        .uxPriority     = mainUNPRIVILEGED_PRIORITY,
        .puxStackBuffer = xPrivilegedUnStack[dom_num - 1],
        .xRegions       =
        {
            { &_text, _erodata - _text, portMPU_REGION_READ | portMPU_REGION_EXECUTE },
            { &_data, _ebss - _data, portMPU_REGION_READ | portMPU_REGION_WRITE },
            /* NS16550 */
            { (void *)0x10000000UL, 0x1000, portMPU_REGION_READ | portMPU_REGION_WRITE },
            { 0, 0, 0 },
            { 0, 0, 0 },
            { 0, 0, 0 },
            { 0, 0, 0 },
            /* *MUST* reserve one entry for stack */
            { 0, 0, 0 },
        }
        #if ( configENABLE_DOMAINS == 1 )
        ,.pxDomainParameters  = NULL,
        #endif
    };

    TaskHandle_t xDomTask;

    BaseType_t ret = pdFAIL;
    while (ret != pdPASS) {
        ret = xTaskCreateRestricted( &( xDomainTaskParameters ), &xDomTask );
    }

    // This is never reached, Privileged has lower priority than UnPrivileged
    for( ; ; )
    {
    }
}

static void domainUnPrivileged(void* pvParameters) {
    uint32_t dom_num = (uint32_t) pvParameters;
    TickType_t domain_tick = xTaskGetDomainTick();
    printf ( "UnPrivileged task: %d, DomainTick: %d\n", dom_num, domain_tick);

    TickType_t prev_tick = xTaskGetTickCount();
    TickType_t cur_tick = xTaskGetTickCount();

    for( ; ; )
    {
        cur_tick = xTaskGetTickCount();
        TickType_t domain_tick = xTaskGetDomainTick();
        if (prev_tick != cur_tick) {
            printf ( "Domain %d, Domain Tick %d\n", dom_num, domain_tick);
            prev_tick = cur_tick;
        }
    }
}


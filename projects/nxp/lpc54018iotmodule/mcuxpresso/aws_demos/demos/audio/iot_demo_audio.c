/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"
#include "semphr.h"

#include "platform/iot_network.h"

#include "AudioPlayer.h"
#include "MicMessage.h"
#include "SpeakerMessage.h"
#include "micBuffer.h"
#include "audio_speaker.h"
#include "fsl_pint.h"

extern void DMIC_getMicData( void * buffer,
                             size_t bufferSize );
extern void playData( size_t dataSize,
                      uint8_t * data );

static AudioPlayer_t g_audioPlayer;
static void schedulePlay( void ( * playTask )( void * ),
                          void * userData );

static TaskHandle_t g_mainTaskHandle = NULL;
static TaskHandle_t g_micTaskHandle = NULL;
static TaskHandle_t g_speakerTaskHandle = NULL;

/* this is a good candidate to pass through the pvUserData */
static volatile int g_micOn = 0;

#define MAX_CURCULAR_BUFFER_SIZE    10000
#define MIC_DATA_SIZE               0x400
#define SPEAKER_THRESHOLD           ( MAX_CURCULAR_BUFFER_SIZE / 2 )

static uint8_t circularBuffer[ MAX_CURCULAR_BUFFER_SIZE ] = { 0 };
static size_t bufferReadHead = 0;
static size_t bufferWriteHead = 0;
static size_t bufferSize = 0;

static void prvMainTask( void * pvParameters );
static void prvMicTask( void * pvParameters );
static void prvSpeakerTask( void * pvParameters );

static void schedulePlay( void ( * playTask )( void * ),
                          void * userData );

int RunAudioDemo( bool awsIotMqttMode,
                  const char * pIdentifier,
                  void * pNetworkServerInfo,
                  void * pNetworkCredentialInfo,
                  const IotNetworkInterface_t * pNetworkInterface )
{
    initMicBuffers();
    initAudioPlayer( &g_audioPlayer, NULL, schedulePlay );
    configPRINTF( ( "Creating ADS main Task...\r\n" ) );

    /* Create the task that publishes messages to the MQTT broker every five
     * seconds.  This task, in turn, creates the task that echoes data received
     * from the broker back to the broker. */
    ( void ) xTaskCreate( prvMainTask,
                          "ADSClientTask",
                          configMINIMAL_STACK_SIZE * 16,
                          NULL,
                          tskIDLE_PRIORITY + 1,
                          &g_mainTaskHandle );
    return 0;
}

static void schedulePlay( void ( * playTask )( void * ),
                          void * userData )
{
    ( void ) xTaskCreate( playTask,                     /* The function that implements the demo task. */
                          "PlayTask",                   /* The name to assign to the task being created. */
                          configMINIMAL_STACK_SIZE * 3, /* The size, in WORDS (not bytes), of the stack to allocate for the task being created. */
                          userData,
                          tskIDLE_PRIORITY + 1,         /* The priority at which the task being created will run. */
                          NULL );                       /* Not storing the task's handle. */
}

static void prvMainTask( void * pvParameters )
{
    /* Avoid compiler warnings about unused parameters. */
    ( void ) pvParameters;

    configPRINTF( ( "Main task created.\r\n" ) );

    BOARD_DMA_EDMA_Start();

    /* TODO -- this clears the notification value before needing to wait. */
    /* It's supposed to be initialized to 0. */
    ulTaskNotifyTake( pdTRUE, ( TickType_t ) 1 );

    configPRINTF( ( "SW2 for joke\r\n" ) );

    while( 1 )
    {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        g_micOn = 1;
        ( void ) xTaskCreate( prvMicTask,                   /* The function that implements the demo task. */
                              "micTask",                    /* The name to assign to the task being created. */
                              configMINIMAL_STACK_SIZE * 3, /* The size, in WORDS (not bytes), of the stack to allocate for the task being created. */
                              NULL,
                              1,                            /* The priority at which the task being created will run. */
                              &g_micTaskHandle );
        configPRINTF( ( "SW2 for joke\r\n" ) );

        ( void ) xTaskCreate( prvSpeakerTask,               /* The function that implements the demo task. */
                              "speakerTask",                /* The name to assign to the task being created. */
                              configMINIMAL_STACK_SIZE * 3, /* The size, in WORDS (not bytes), of the stack to allocate for the task being created. */
                              NULL,
                              1,                            /* The priority at which the task being created will run. */
                              &g_speakerTaskHandle );
    }

    deinitAudioPlayer( &g_audioPlayer );
    vTaskDelete( NULL ); /* Delete this task. */
}

static void prvMicTask( void * pvParameters )
{
    ( void ) pvParameters;
    configPRINTF( ( "MIC task started.\r\n" ) );

    /* TODO --- clear the notification value */
    ulTaskNotifyTake( pdTRUE, ( TickType_t ) 1 );

    while( 1 )
    {
        if( ( MAX_CURCULAR_BUFFER_SIZE - bufferSize ) >= MIC_DATA_SIZE )
        {
            if( bufferWriteHead + MIC_DATA_SIZE <= MAX_CURCULAR_BUFFER_SIZE )
            {
                DMIC_getMicData( circularBuffer + bufferWriteHead, MIC_DATA_SIZE );
                ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

                bufferWriteHead += MIC_DATA_SIZE;
            }
            else
            {
                if( bufferWriteHead < MAX_CURCULAR_BUFFER_SIZE )
                {
                    DMIC_getMicData( circularBuffer + bufferWriteHead, MAX_CURCULAR_BUFFER_SIZE - bufferWriteHead );
                    ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
                }

                DMIC_getMicData( circularBuffer, MIC_DATA_SIZE - ( MAX_CURCULAR_BUFFER_SIZE - bufferWriteHead ) );
                ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

                bufferWriteHead = MIC_DATA_SIZE - ( MAX_CURCULAR_BUFFER_SIZE - bufferWriteHead );
            }

            bufferSize += MIC_DATA_SIZE;

            if( bufferSize >= SPEAKER_THRESHOLD )
            {
                xTaskNotifyGive( g_speakerTaskHandle );
            }
        }
        else
        {
            vTaskDelay( pdMS_TO_TICKS( 1000 ) );
        }
    }

    configPRINTF( ( "MIC task finished.\r\n" ) );
    vTaskDelete( NULL ); /* Delete this task. */
}

static void prvSpeakerTask( void * pvParameters )
{
    int i = 0;

    while( 1 )
    {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

        if( bufferReadHead + SPEAKER_THRESHOLD <= MAX_CURCULAR_BUFFER_SIZE )
        {
            for( i = bufferReadHead; i < bufferReadHead + SPEAKER_THRESHOLD; i++ )
            {
                circularBuffer[ i ] <<= 1;
            }

            playData( SPEAKER_THRESHOLD, circularBuffer + bufferReadHead );
            bufferReadHead += SPEAKER_THRESHOLD;
        }
        else
        {
            for( i = bufferReadHead; i < MAX_CURCULAR_BUFFER_SIZE; i++ )
            {
                circularBuffer[ i ] <<= 1;
            }

            for( i = 0; i < SPEAKER_THRESHOLD - ( MAX_CURCULAR_BUFFER_SIZE - bufferReadHead ); i++ )
            {
                circularBuffer[ i ] <<= 1;
            }

            playData( MAX_CURCULAR_BUFFER_SIZE - bufferReadHead, circularBuffer + bufferReadHead );
            playData( SPEAKER_THRESHOLD - ( MAX_CURCULAR_BUFFER_SIZE - bufferReadHead ), circularBuffer );

            bufferReadHead = SPEAKER_THRESHOLD - ( MAX_CURCULAR_BUFFER_SIZE - bufferReadHead );
        }

        bufferSize -= SPEAKER_THRESHOLD;
    }

    configPRINTF( ( "Speaker task finished.\r\n" ) );
    vTaskDelete( NULL ); /* Delete this task. */
}

void button_callback(pint_pin_int_t pintr, uint32_t pmatch_status)
{
    BaseType_t xHigherPriorityTaskWoken;

    /* xHigherPriorityTaskWoken must be initialised to pdFALSE.
     * If calling vTaskNotifyGiveFromISR() unblocks the handling
     * task, and the priority of the handling task is higher than
     * the priority of the currently running task, then
     * xHigherPriorityTaskWoken will be automatically set to pdTRUE. */
    xHigherPriorityTaskWoken = pdFALSE;

    /* Unblock the handling task so the task can perform any processing
     * necessitated by the interrupt.  xHandlingTask is the task's handle, which was
     * obtained when the task was created.  vTaskNotifyGiveFromISR() also increments
     * the receiving task's notification value. */
    vTaskNotifyGiveFromISR
        ( g_mainTaskHandle, &xHigherPriorityTaskWoken );

    /* Force a context switch if xHigherPriorityTaskWoken is now set to pdTRUE.
     * The macro used to do this is dependent on the port and may be called
     * portEND_SWITCHING_ISR. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void transferComplete()
{
    BaseType_t xHigherPriorityTaskWoken;

    /* xHigherPriorityTaskWoken must be initialised to pdFALSE.
     * If calling vTaskNotifyGiveFromISR() unblocks the handling
     * task, and the priority of the handling task is higher than
     * the priority of the currently running task, then
     * xHigherPriorityTaskWoken will be automatically set to pdTRUE. */
    xHigherPriorityTaskWoken = pdFALSE;

    /* Unblock the handling task so the task can perform any processing
     * necessitated by the interrupt.  xHandlingTask is the task's handle, which was
     * obtained when the task was created.  vTaskNotifyGiveFromISR() also increments
     * the receiving task's notification value. */
    vTaskNotifyGiveFromISR
        ( g_micTaskHandle, &xHigherPriorityTaskWoken );

    /* Force a context switch if xHigherPriorityTaskWoken is now set to pdTRUE.
     * The macro used to do this is dependent on the port and may be called
     * portEND_SWITCHING_ISR. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

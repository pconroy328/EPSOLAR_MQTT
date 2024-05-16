/*
 * File:    jsonMessagemaker.c
 * author:  patrick conroy
 * 
 * The goal is to crate a JSON message that has a structure like this:
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cjson/cJSON.h>
#include "log4c.h"
#include "libepsolar.h"


extern char    *getCurrentDateTime( void );
extern  int     sendExtraData;

//
//  Quickie Macros to control Floating Point Precision
#define FP21P(x) ( (((int)((x) * 10 + .5))/10.0) )                  // Floating Point to 1 point
#define FP22P(x) ( (((int)((x) * 100 + .5))/100.0) )                // Floating Point to 2 points
#define FP23P(x) ( (((int)((x) * 1000 + .5))/1000.0) )              // Floating Point to 3 points
#define FP24P(x) ( (((int)((x) * 10000 + .5))/10000.0) )            // Floating Point to 4 points




// -----------------------------------------------------------------------------
static  char    currentDateTimeBuffer[ 80 ];
char    *getCurrentDateTime (void)
{
    //
    // Something quick and dirty... Fix this later - thread safe
    time_t  current_time;
    struct  tm  *tmPtr;
 
    memset( currentDateTimeBuffer, '\0', sizeof currentDateTimeBuffer );
    
    /* Obtain current time as seconds elapsed since the Epoch. */
    current_time = time( NULL );
    if (current_time > 0) {
        /* Convert to local time format. */
        tmPtr = localtime( &current_time );
 
        if (tmPtr != NULL) {
            strftime( currentDateTimeBuffer,
                    sizeof currentDateTimeBuffer,
                    "%FT%T%z",                           // ISO 8601 Format
                    tmPtr );
        }
    }
    
    return &currentDateTimeBuffer[ 0 ];
}

// -----------------------------------------------------------------------------
char *realTimeDataToJSON (const char *topic, const const epsolarRealTimeData_t *rtData)
{
    //
    //  Dave Gamble's C library to create JSON
    cJSON *message = cJSON_CreateObject();

    cJSON_AddStringToObject( message, "topic", topic );
    cJSON_AddStringToObject( message, "version", "4.0" );
    
    //
    //  cJSON uses a "%1.15g" format for number formatting which means we can get some
    //  very large FP numbers in the output.  Let's round and truncate before we have cJSON
    //  format the numbers.
    //
    cJSON_AddStringToObject( message, "dateTime", getCurrentDateTime() );
    cJSON_AddStringToObject( message, "controllerDateTime", rtData-> controllerClock );
    cJSON_AddBoolToObject( message, "isNightTime", rtData->isNightTime );
    cJSON_AddBoolToObject( message, "loadIsOn", rtData->loadIsOn );
    
    cJSON_AddNumberToObject( message, "pvVoltage", FP22P( rtData->pvVoltage ) );
    cJSON_AddNumberToObject( message, "pvCurrent", FP22P( rtData->pvCurrent ) );
    cJSON_AddNumberToObject( message, "pvPower", FP22P( rtData->pvPower ) );
    cJSON_AddStringToObject( message, "pvStatus", rtData->pvStatus );
    
    cJSON_AddNumberToObject( message, "loadVoltage", FP22P( rtData->loadVoltage ) );
    cJSON_AddNumberToObject( message, "loadCurrent", FP22P( rtData->loadCurrent ) );
    cJSON_AddNumberToObject( message, "loadPower", FP22P( rtData->loadPower ) );
    cJSON_AddStringToObject( message, "loadLevel", rtData->loadLevel );
    cJSON_AddStringToObject( message, "loadControlMode", rtData->loadControlMode );

    cJSON_AddNumberToObject( message, "batterySOC", rtData->batteryStateOfCharge );
    cJSON_AddNumberToObject( message, "batteryVoltage", FP22P( rtData->batteryVoltage ) );
    cJSON_AddNumberToObject( message, "batteryCurrent", FP22P( rtData->batteryCurrent ) );
    cJSON_AddStringToObject( message, "batteryStatus", rtData->batteryStatus );
    cJSON_AddNumberToObject( message, "batteryMaxVoltage", FP22P( rtData->batteryMaxVoltage ) );
    cJSON_AddNumberToObject( message, "batteryMinVoltage", FP22P( rtData->batteryMinVoltage ) );
    cJSON_AddStringToObject( message, "batteryChargingStatus", rtData->batteryChargingStatus );
    
    //
    // Been seeing some spurious values coming thru. We'll ignore them from now on
    if ( (rtData->batteryTemperature >= -50.0) && (rtData->batteryTemperature <= 150.0))
        cJSON_AddNumberToObject( message, "batteryTemperature", FP21P( rtData->batteryTemperature ) );
    else 
        Logger_LogWarning( "Battery Temperature out of range. Ignoring: %f\n", rtData->batteryTemperature );
    
    if ( (rtData->controllerTemp >= -50.0) && (rtData->controllerTemp <= 150.0))
        cJSON_AddNumberToObject( message, "controllerTemperature", FP21P( rtData->controllerTemp ) );
    else 
        Logger_LogWarning( "Controller Temperature out of range. Ignoring: %f\n", rtData->controllerTemp );
        
    cJSON_AddStringToObject( message, "chargerStatusNormal", (rtData->chargerStatusNormal ? "Yes" : "No" ));
    cJSON_AddStringToObject( message, "chargerRunning", (rtData->chargerRunning ? "Yes" : "No" ));
    cJSON_AddNumberToObject( message, "deviceArrayChargingStatusBits", rtData->controllerStatusBits );
    
    cJSON_AddNumberToObject( message, "energyConsumedToday", FP22P( rtData->energyConsumedToday ) );
    cJSON_AddNumberToObject( message, "energyConsumedMonth", FP22P( rtData->energyConsumedMonth ) );
    cJSON_AddNumberToObject( message, "energyConsumedYear", FP22P( rtData->energyConsumedYear ) );
    cJSON_AddNumberToObject( message, "energyConsumedTotal", FP22P( rtData->energyConsumedTotal ) );
    cJSON_AddNumberToObject( message, "energyGeneratedToday", FP22P( rtData->energyGeneratedToday ) );
    cJSON_AddNumberToObject( message, "energyGeneratedMonth", FP22P( rtData->energyGeneratedMonth ) );
    cJSON_AddNumberToObject( message, "energyGeneratedYear", FP22P( rtData->energyGeneratedYear ) );
    cJSON_AddNumberToObject( message, "energyGeneratedTotal", FP22P( rtData->energyGeneratedTotal ) );

    //
    //  New - let's see if we can pull some other data out
    if (sendExtraData) {
        cJSON_AddNumberToObject( message, "BatteryRealRatedVoltage", FP22P(eps_getBatteryRealRatedVoltage() ) );
        uint16_t bits = eps_getBatteryStatusBits();

        cJSON_AddStringToObject( message, "BatteryRatedVoltageCode", eps_getBatteryRatedVoltageCode() );
        cJSON_AddStringToObject( message, "BatteryStatusInnerResistance", eps_getBatteryStatusInnerResistance( bits ) );
        cJSON_AddStringToObject( message, "BatteryStatusIdentification", eps_getBatteryStatusIdentification( bits ) );

        bits = eps_getChargingEquipmentStatusBits();
        cJSON_AddStringToObject( message, "ChargingEquipmentStatusInputVoltageStatus", eps_getChargingEquipmentStatusInputVoltageStatus( bits ) );
        cJSON_AddStringToObject( message, "ChargingEquipmentStatusInputVoltageStatus", eps_getChargingEquipmentStatusInputVoltageStatus( bits ) );


        cJSON_AddBoolToObject( message, "isChargingMOSFETShorted", isChargingMOSFETShorted( bits ) );
        cJSON_AddBoolToObject( message, "isChargingMOSFETOpen", isChargingMOSFETOpen( bits ) );
        cJSON_AddBoolToObject( message, "isAntiReverseMOSFETShort", isAntiReverseMOSFETShort( bits ) );
        cJSON_AddBoolToObject( message, "isInputOverCurrent", isInputOverCurrent( bits ) );
        cJSON_AddBoolToObject( message, "isLoadOverCurrent", isLoadOverCurrent( bits ) );
        cJSON_AddBoolToObject( message, "isLoadShorted", isLoadShorted( bits ) );
        cJSON_AddBoolToObject( message, "isLoadMOSFETShorted", isLoadMOSFETShorted( bits ) );
        cJSON_AddBoolToObject( message, "isDisequilibriumInThreeCircuits", isDisequilibriumInThreeCircuits( bits ) );
        cJSON_AddBoolToObject( message, "isPVInputShorted", isPVInputShorted( bits ) );

        bits = eps_getDischargingEquipmentStatusBits();
        cJSON_AddBoolToObject( message, "isDischargeStatusShorted", isDischargeStatusShorted( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusUnableToDischarge", isDischargeStatusUnableToDischarge( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusUnableToStopDischarge", isDischargeStatusUnableToStopDischarge( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusOutputVoltageAbnormal", isDischargeStatusOutputVoltageAbnormal( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusInputOverVoltage", isDischargeStatusInputOverVoltage( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusShortedInHighVoltage", isDischargeStatusShortedInHighVoltage( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusBoostOverVoltage", isDischargeStatusBoostOverVoltage( bits ) );
        cJSON_AddBoolToObject( message, "isPVisDischargeStatusOutputOverVoltageInputShorted", isDischargeStatusOutputOverVoltage( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusNormal", isDischargeStatusNormal( bits ) );
        cJSON_AddBoolToObject( message, "isDischargeStatusRunning", isDischargeStatusRunning( bits ) );


        cJSON_AddNumberToObject( message, "RatedChargingCurrent", FP22P(eps_getRatedChargingCurrent() ) );
        cJSON_AddNumberToObject( message, "RatedLoadCurrent", FP22P(eps_getRatedLoadCurrent() ) );

        cJSON_AddNumberToObject( message, "BoostDuration", eps_getBoostDuration() );
        cJSON_AddNumberToObject( message, "EqualizeDuration", eps_getEqualizeDuration() );


        cJSON_AddNumberToObject( message, "RatedChargingCurrent", FP22P(eps_getRatedChargingCurrent() ) );
        cJSON_AddStringToObject( message, "BatteryType", eps_getBatteryType() );
        cJSON_AddNumberToObject( message, "BatteryCapacity", eps_getBatteryCapacity() );

        cJSON_AddNumberToObject( message, "HighVoltageDisconnect", FP22P(eps_getHighVoltageDisconnect() ) );
        cJSON_AddNumberToObject( message, "ChargingLimitVoltage", FP22P(eps_getChargingLimitVoltage() ) );
        cJSON_AddNumberToObject( message, "OverVoltageReconnect", FP22P(eps_getOverVoltageReconnect() ) );

        cJSON_AddNumberToObject( message, "EqualizationVoltage", FP22P(eps_getEqualizationVoltage() ) );
        cJSON_AddNumberToObject( message, "BoostingVoltage", FP22P(eps_getBoostingVoltage() ) );
        cJSON_AddNumberToObject( message, "FloatingVoltage", FP22P(eps_getFloatingVoltage() ) );
        cJSON_AddNumberToObject( message, "BoostReconnectVoltage", FP22P(eps_getBoostReconnectVoltage() ) );

        cJSON_AddNumberToObject( message, "LowVoltageReconnectVoltage", FP22P(eps_getLowVoltageReconnectVoltage() ) );
        cJSON_AddNumberToObject( message, "UnderVoltageWarningRecoverVoltage", FP22P(eps_getUnderVoltageWarningRecoverVoltage() ) );
        cJSON_AddNumberToObject( message, "UnderVoltageWarningVoltage", FP22P(eps_getUnderVoltageWarningVoltage() ) );
        cJSON_AddNumberToObject( message, "LowVoltageDisconnectVoltage", FP22P(eps_getLowVoltageDisconnectVoltage() ) );
        cJSON_AddNumberToObject( message, "DischargingLimitVoltage", FP22P(eps_getDischargingLimitVoltage() ) );

        cJSON_AddNumberToObject( message, "DischargingPercentage", FP22P(eps_getDischargingPercentage() ) );
        cJSON_AddNumberToObject( message, "ChargingPercentage", FP22P(eps_getChargingPercentage() ) );

        cJSON_AddNumberToObject( message, "BatteryTemperatureWarningUpperLimit", FP22P(eps_getBatteryTemperatureWarningUpperLimit() ) );
        cJSON_AddNumberToObject( message, "BatteryTemperatureWarningLowerLimit", FP22P(eps_getBatteryTemperatureWarningLowerLimit() ) );

        cJSON_AddNumberToObject( message, "ControllerInnerTemperatureUpperLimit", FP22P(eps_getControllerInnerTemperatureUpperLimit() ) );
        cJSON_AddNumberToObject( message, "ControllerInnerTemperatureUpperLimitRecover", FP22P(eps_getControllerInnerTemperatureUpperLimitRecover() ) );
        cJSON_AddNumberToObject( message, "BatteryTemperatureWarningLowerLimit", FP22P(eps_getBatteryTemperatureWarningLowerLimit() ) );
    }
    
    //
    //  From the cJSON notes: Important: If you have added an item to an array 
    //  or an object already, you mustn't delete it with cJSON_Delete. Adding 
    //  it to an array or object transfers its ownership so that when that 
    //  array or object is deleted, it gets deleted as well.
    //char *string = cJSON_Print( message );
    char *string = cJSON_PrintUnformatted( message );
    cJSON_Delete( message );
    
    return string;
}
/*
 */


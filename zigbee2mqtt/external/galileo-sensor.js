// from https://github.com/Koenkk/zigbee-herdsman-converters/blob/cbaa6f45dcb61822ed7a44493ec304b386aaf08a/src/devices/efekta.ts#L54
// https://github.com/Koenkk/zigbee-herdsman-converters/blob/master/src/lib/modernExtend.ts

const { identify, onOff, numeric, temperature, commandsOnOff } = require('zigbee-herdsman-converters/lib/modernExtend');
//const reporting = require('zigbee-herdsman-converters/lib/reporting');

// const dataType = {
//     boolean: 0x10,
//     uint8: 0x20,
//     uint16: 0x21,
//     int8: 0x28,
//     int16: 0x29,
//     enum8: 0x30,
// };
//const defaultReporting = { min: 0, max: 300, change: 1 };

const definition = {
    zigbeeModel: ['galileo.sensor'],
    model: 'galileo.sensor',
    vendor: 'Galileo',
    description: 'Galileo generic sensor',
    extend: [
        identify(),
        onOff({ "powerOnBehavior": false }),
        commandsOnOff({commands: ['toggle'], bind: false}),
        temperature(),
        numeric({
            name: 'battery voltage',
            cluster: 'genAnalogInput',
            unit: 'mV',
            attribute: 'presentValue',
            reporting: { min: 0, max: 10, change: 5 },
            description: 'battery voltage',
            access: 'STATE_GET',
        }),
        numeric({
            name: 'solar panel voltage',
            cluster: 'genAnalogInput',
            unit: 'mV',
            attribute: 'presentValue',
            reporting: { min: 0, max: 10, change: 5 },
            description: 'solar panel voltage',
            access: 'STATE_GET',
            endpointNames: ["ep2"]
        }),
        // numeric({
            //     name: 'xyz',
        //     cluster: '65522', // important
        //     attribute: { ID: 0x0, type: dataType.uint16 },
        //     description: 'custom value',
        // }),
        // numeric({
        //     name: 'report_delay',
        //     unit: 'min',
        //     valueMin: 1,
        //     valueStep: 0.1,
        //     valueMax: 240,
        //     cluster: 'genPowerCfg',
        //     attribute: {ID: 0x0201, type: Zcl.DataType.UINT16},
        //     description: 'Adjust Report Delay. Setting the time in minutes, by default 15 minutes',
        // }),
    ],
    meta: {},
    endpoint: (device) => { return {'ep1': 10, 'ep2': 11}; }, // https://github.com/Koenkk/zigbee2mqtt/blob/df0543d4b7d04578c5000a889090259d3abcb32f/lib/util/utils.ts#L30
};

module.exports = definition;


// // Add the lines below
// const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
// const tz = require('zigbee-herdsman-converters/converters/toZigbee');
// const exposes = require('zigbee-herdsman-converters/lib/exposes');
// const reporting = require('zigbee-herdsman-converters/lib/reporting');
// const extend = require('zigbee-herdsman-converters/lib/extend');
// const ota = require('zigbee-herdsman-converters/lib/ota');
// const tuya = require('zigbee-herdsman-converters/lib/tuya');
// const { } = require('zigbee-herdsman-converters/lib/tuya');
// const utils = require('zigbee-herdsman-converters/lib/utils');
// const globalStore = require('zigbee-herdsman-converters/lib/store');
// const e = exposes.presets;
// const ea = exposes.access;

// const fLocal = {
//     windSpeed: {
//         cluster: '65534', //This part is important
//         type: ['readResponse', 'attributeReport'],
//         convert: (model, msg, publish, options, meta) →> {
//         if(msg.data.hasOwnProperty('1')) {
//             const windSpeed = parseFloat(msg - data['1']);
// return { I'windSpeed']: windSpeed };
// 		}
// 	},
// };

// const definition = {
//     zigbeeModel: ['MetaWeather'],
//     model: 'WeatherStation', // Update this with the real model of the device (written on the device itself or product page) 
//     vendor: 'MetaImi', // Update this with the real vendor of the device (written on the device itself or product page) 
//     description: 'Weather station by MetaImi', // Description of the device, copy from vendor site. (only used for documentation and startup logging)
//     extend: [],
//     fromZigbee: [fLocal.windSpeed, fz.temperature, fz.humidity],
//     toZigbee: [],
//     exposes: [
//         e.temperature(), 
//         e.humidity(), 
//         exposes.enum('Wind Direction', ea.STATE, ['north', 'east', 'west']).withDescription('Wind direction'), e.numeric('windSpeed', ea.STATE).withLabel('Wind speed').withUnit('km/h').withDescription('Measured wind speed')],
// };

// module.exports = definition;
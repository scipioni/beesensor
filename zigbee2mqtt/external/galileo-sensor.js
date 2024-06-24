// from https://github.com/Koenkk/zigbee-herdsman-converters/blob/cbaa6f45dcb61822ed7a44493ec304b386aaf08a/src/devices/efekta.ts#L54

const {identify, onOff, numeric} = require('zigbee-herdsman-converters/lib/modernExtend');

const defaultReporting = {min: 0, max: 300, change: 1};

const definition = {
    zigbeeModel: ['galileo.sensor'],
    model: 'galileo.sensor',
    vendor: 'Galileo',
    description: 'Galileo generic sensor',
    extend: [
        identify(), 
        onOff({"powerOnBehavior":false}),
        numeric({
            name: 'analog_value',
            unit: 'analog value',
            cluster: 'genAnalogValue',
            attribute: 'presentValue',
            description: 'analog value',
            access: 'STATE',
            //reporting: defaultReporting, // lato sensore deve essere implementato il comando
        }),
        numeric({
            name: 'temperature',
            cluster: 'msTemperatureMeasurement',
            attribute: 'measuredValue',
            reporting: {min: '5_SECONDS', max: '10_SECONDS', change: 1},
            description: 'Measured temperature value',
            unit: '°C',
            scale: 100,
            access: 'STATE_GET',
        }),
        // numeric({
        //     name: 'report_delay',
        //     unit: 'min',
        //     valueMin: 1,
        //     valueMax: 240,
        //     cluster: 'genPowerCfg',
        //     attribute: {ID: 0x0201, type: Zcl.DataType.UINT16},
        //     description: 'Adjust Report Delay. Setting the time in minutes, by default 15 minutes',
        // }),
    ],
    meta: {},
};

module.exports = definition;

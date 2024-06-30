const {identify, temperature} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['galileo.temp'],
    model: 'galileo.temp',
    vendor: 'Galileo',
    description: 'minimal temperature sensor. Push boot button to report single value',
    extend: [
        identify(), 
        numeric({
            name: 'temperature',
            cluster: 'msTemperatureMeasurement',
            attribute: 'measuredValue',
            reporting: {min: 10, max: 60, change: 100},
            description: 'Measured temperature value',
            unit: '°C',
            scale: 100,
            access: 'STATE_GET',
        })
    ],
    meta: {},
};

module.exports = definition;

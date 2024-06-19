const {identify, onOff} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['ESP32C6.Light'],
    model: 'ESP32C6.Light',
    vendor: 'Espressif',
    description: 'Automatically generated definition',
    extend: [identify(), onOff({"powerOnBehavior":false}],
    meta: {},
};

module.exports = definition;

const {identify, onOff} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['galileo.sensor'],
    model: 'galileo.sensor',
    vendor: 'Galileo',
    description: 'Galileo generic sensor',
    extend: [identify(), onOff({"powerOnBehavior":false})],
    meta: {},
};

module.exports = definition;

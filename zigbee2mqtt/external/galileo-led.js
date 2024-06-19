const {identify, onOff} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['galileo.led'],
    model: 'galileo.led',
    vendor: 'Galileo',
    description: 'Galileo Led',
    extend: [identify(), onOff({"powerOnBehavior":false})],
    meta: {},
};

module.exports = definition;

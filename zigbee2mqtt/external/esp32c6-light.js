const {identify, onOff} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['Galileo.Led'],
    model: 'Galileo.Led',
    vendor: 'Galileo',
    description: 'Led device',
    fromZigbee: [fz.on_off],
    toZigbee: [tz.on_off],
    exposes: [e.switch()],
    // The configure method below is needed to make the device reports on/off state changes
    // when the device is controlled manually through the button on it.
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        await reporting.bind(endpoint, coordinatorEndpoint, ['genOnOff']);
        await reporting.onOff(endpoint);
    },
};

module.exports = definition;

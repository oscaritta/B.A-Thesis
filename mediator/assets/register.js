var dgram = require('dgram');
var net = require('net');

function registerUDPClient(serverIp, serverPort, localPort, id, password, onfinish) {
    var s = dgram.createSocket('udp4');
    s.bind(localPort);
    s.on('message', function(msg, rinfo) {
        if (msg.indexOf("ports") == 0)
            s.close();
        onfinish(msg.toString());

        setTimeout( () => {
            if (msg == "ok") {
                var message = 'get_server_udp_port ' + id + ' ' + password;
                s.send(message, 0, message.length, serverPort, serverIp, (err, bytes) => {
                    if (err) throw err;
                    console.log(message + ' sent');
                });
            }
        }, 5000);
    });
    message = 'register_udp_client '+localPort+' '+id+' '+password;
    message = message.toString('ascii')
    s.send(message, 0, message.length, serverPort, serverIp, (err, bytes) => {
        if (err) throw err;
        console.log(message + ' sent');
    });
}

function registerUDPServer(serverIp, serverPort, localPort, id, password, onfinish) {
    var s = dgram.createSocket('udp4');
    s.bind(localPort);
    s.on('message', function(msg, rinfo) {
        if (msg.indexOf("ports") == 0)
            s.close();
        onfinish(msg.toString());

        setTimeout( () => {
            if (msg == "ok") {
                var message = 'get_client_udp_port ' + id + ' ' + password;
                s.send(message, 0, message.length, serverPort, serverIp, (err, bytes) => {
                    if (err) throw err;
                    console.log(message + ' sent');
                });
            }
        }, 5000);

    });
    message = 'register_udp_server '+localPort+' '+id+' '+password;
    message = message.toString('ascii')
    s.send(message, 0, message.length, serverPort, serverIp, (err, bytes) => {
        if (err) throw err;
        console.log(message + ' sent');
    });
}

function registerTCPSession(serverIp, serverPort, localIp, localPort, onfinish) {
    var client = new net.Socket();
    client.localPort = localPort;
    client.localAddress = '0.0.0.0';
    client.connect({port:serverPort, host:serverIp, localPort:localPort}, () => {
        console.log('Connected');
        client.write('register ' + localIp + ' ' + localPort);
    });
    client.on('data', (data) => {
        console.log('received data: ' + data.toString());
        client.end();
        client.close();
    })
}

module.exports.registerUDPClient = registerUDPClient;
module.exports.registerUDPServer = registerUDPServer;
module.exports.registerTCPSession = registerTCPSession;

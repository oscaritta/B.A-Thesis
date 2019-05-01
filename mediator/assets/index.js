const http = require('http');
const queryString = require('querystring');
const {exec,spawn} = require('child_process');
const fs = require('fs');
const Swal = require('sweetalert2');
const request = require('request');
const net = require('net');
const dgram = require('dgram');
const getRandomInt = require('./assets/js/utils.js');
const {registerUDPClient,registerUDPServer,registerTCPSession} = require('./assets/js/register.js');
const electron = require('electron');
const {ipcRenderer} = electron;

function getUDPPorts(type, iplocal, ipglobal, portlocal, portglobal, id, password, onfinish) {

    function handleResponse(data, type, iplocal, ipglobal, portlocal, portglobal, id, password, onfinish) {
        if (data == 'ok') {
            console.log(type+': ok');
        } else if (data == 'incorrect password') {
            Swal(type+': Incorrect password');
        } else if (data == 'not enough arguments') {
            Swal(type+': not enough arguments');
        } else if (data == 'id not found') { 
            Swal(type+': id not found')
        } else if (data == 'udp not ready') {
            Swal(type+': udp not ready. Try to reload the program');
        } else if (data == 'unknown command') {
            Swal(type+': unknown command');
        } else if (data.indexOf('ports') == 0) {
            var port_local_udp = data.split(" ")[1];
            var port_remote_udp = data.split(" ")[2];
            console.log('Ports are ' + port_local_udp + ' ' + port_remote_udp);

            var teamv_type_launch = type == "server" ? "server" : "client";
            var listen_port = type == "server" ? myPortServer : myPortClient;
            
            if (type == "client") {
                waitForRemoteServer(() => {
                    console.log("waitForRemoteServer onfinish called for client");
                    const clientProcess = spawn('TeamV.exe ' + teamv_type_launch + ' ' +iplocal+" "+ipglobal+" "+portlocal+" "+portglobal+" "+port_local_udp + " " + port_remote_udp + " " + listen_port, {shell:true,detached:true})
                    onfinish();
                    $.unblockUI();

                    
                    ipcRenderer.send('connection_established', {dummy:undefined});
                });
            }
            else {
                const serverProcess = spawn('TeamV.exe ' + teamv_type_launch + ' ' +iplocal+" "+ipglobal+" "+portlocal+" "+portglobal+" "+port_local_udp + " " + port_remote_udp + " " + listen_port, {shell:true,detached:true})
                waitForRemoteServer(() => {
                    console.log("waitForRemoteServer onfinish called for server");
                    onfinish();
                    $.unblockUI();
                    
                    ipcRenderer.send('connection_established', {dummy:undefined});
                });

            }
        }
    }

    if (type == "server")
        registerUDPServer('157.230.29.114', 4123, myPortServer, id, password, (msg) => {
            handleResponse(msg, type, iplocal, ipglobal, portlocal, portglobal, id, password, onfinish);
        });
    else 
        registerUDPClient('157.230.29.114', 4123, myPortClient, id, password, (msg) => {
            handleResponse(msg, type, iplocal, ipglobal, portlocal, portglobal, id, password, onfinish);
        });

    $.blockUI(); 
}

function waitForRemoteServer(onready) {
    console.log('launching connection');
    const host = '157.230.29.114';
    const port = 4123;
    var client = net.connect({host:host, port:port, localPort:5000+getRandomInt(1000), localAddress:'0.0.0.0'}, () => {
        console.log('connected to mediator server');
        setTimeout( () => {client.write('isready');}, 2000);
    });
    client.on('error', (data) => {
        Swal(data.toString());
    })
    client.on('data', (data) => {
        var strData = data.toString();
        console.log(host+':'+port+' -> ' + strData);

        if (strData == 'no') {
            setTimeout( () => {client.write('isready');}, 1000);
        } else if (strData == 'yes') {
            onready();
            console.log('Remote PC Is Ready');
        } else {
            return;
        }
    })
}

let myPortServer = 5000+getRandomInt(1000);
let myPortClient = undefined;

function processResponse(data) {
    if (data.indexOf("credentials") == 0) {
        var id, password;
        id = data.split(" ")[1].split(";")[0];
        password = data.split(" ")[1].split(";")[1];
        document.querySelector("#userid").innerHTML = id;
        document.querySelector("#userpwd").innerHTML = password;
    }
    else if (data.indexOf("already registered") == 0) {
        Swal(data);
    }
    else if (data.indexOf("request") == 0) {
        console.log('mediator: ' + data)
        var iplocal, portlocal;
        var ipglobal, portglobal;
        var addr = data.split(" ")[1].split(";");
        iplocal = addr[0];
        portlocal = addr[1];
        ipglobal = addr[2];
        portglobal = addr[3];

        var id = document.querySelector("#userid").innerHTML;
        var password = document.querySelector("#userpwd").innerHTML;
        var arg = ""+myPortServer+" "+id+" "+password;
        console.log("server_udp.py arg is " + arg);

        ipcRenderer.send("chat_channel", {id:id, password:password, type:'server'});

        /* Launching UDP Exchange Script */
        getUDPPorts("server", iplocal, ipglobal, portlocal, portglobal, id,password, () => {
            console.log("server onfinish called");
        });
    }
}

function registerServer() {
    const registerProcess = spawn('python', [ 'register.py', ""+myPortServer], {shell:true});
    
    registerProcess.stderr.on('data', (data) => {
        Swal({title: 'Error', text: data});
    });

    fs.watchFile('mediator.txt', {interval:100}, (curr, prev) => {
        fs.readFile('mediator.txt', function(err, data){
            fs.truncateSync('mediator.txt');
            processResponse(data.toString());
        });
    });

    registerProcess.stdout.on('data', (data) => {
        console.log('register.py stdout: ' + data);
    });
}

function connect() {
    myPortClient = 6000+getRandomInt(1000);

    Swal({
        title: "Enter Password",
        input: 'password',
        confirmButtonText: 'Connect',
        showLoaderOnConfirm: true,
        preConfirm: (password) => {
            const registerProcess = spawn('python', [ 'connect.py', ""+myPortClient+" "+document.querySelector("#remoteid").value+" "+password], {shell:true});
            registerProcess.stderr.on('data', (chunk) => { Swal(chunk); });

            fs.watchFile('connect_resp.txt', {interval:100}, (curr, prev) => {
                fs.readFile('connect_resp.txt', function(err, data){
                    if (data.toString() == "sender not found") {
                        Swal({title: 'Error', text: 'The ID you are trying to connect to does not exist'});
                    } else if (data.toString() == "incorrect password") {
                        Swal({title: 'Error', text: 'Incorrect password'});
                    } else if (data.toString().indexOf("address") == 0) {
                        var iplocal, portlocal;
                        var ipglobal, portglobal;
                        var addr = data.toString().split(" ")[1].split(";");
                        iplocal = addr[0];
                        portlocal = addr[1];
                        ipglobal = addr[2];
                        portglobal = addr[3];
                        //console.log("Client connects to " + iplocal);
                        //console.log("Client connects to " + portlocal);
                        //console.log("Client connects to " + ipglobal);
                        //console.log("Client connects to " + portglobal);
                        
                        ipcRenderer.send("chat_channel", {id:document.querySelector("#remoteid").value, password:password, type:'client'});

                        getUDPPorts("client", iplocal, ipglobal, portlocal, portglobal, document.querySelector("#remoteid").value,password, () => {
                            console.log("client onfinish called");
                            
                        }); 

                    }
                    else Swal(data.toString());
                });
            });
        }
    });
}


registerServer();
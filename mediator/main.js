// Modules to control application life and create native browser window
const {app, BrowserWindow, ipcMain} = require('electron');
const http = require('http');
const fs = require('fs');
const net = require('net');
const dgram = require('dgram');
const getRandomInt = require('./assets/js/utils.js');

let screenSharingServerState;
let chatChannel;

fs.truncateSync('readytoserve.txt');
fs.watchFile('readytoserve.txt', {interval:100}, (curr, prev) => {
  fs.readFile('readytoserve.txt', function(err, data){
      if (data.toString() == "yes")
        screenSharingServerState = true;

        //Notify the server that we are ready to accept connections
        const host = '157.230.29.114';
        const port = 4123;
        var client = net.connect({host:host, port:port, localPort:5000+getRandomInt(1000), localAddress:'0.0.0.0'}, () => {
            console.log('connected to mediator server');
            setTimeout( () => {client.write('iamready');}, 2000);
        });
        client.on('error', (data) => {
            Swal(data.toString());
        })
        client.on('data', (data) => {
            var strData = data.toString();
            console.log(host+':'+port+' -> ' + strData);
        })

  });
});


// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow

function createWindow () {
  // Create the browser window.
  mainWindow = new BrowserWindow({width: 800, height: 500, resizable:false})

  // and load the index.html of the app.
  mainWindow.loadFile('index.html')
  // Open the DevTools.
  // mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null
  })

  ipcMain.on('connection_established', (e, item) => {
    mainWindow.loadFile('chat.html')
    mainWindow.setSize(400,500);

    setTimeout ( () => {
      console.log("sending chat channel to chat.html")
      mainWindow.webContents.send('chat_channel_final', chatChannel); 
    }, 3000);

  });
  ipcMain.on('chat_channel', (e, item) => {
    console.log("Chat channel is: ", item);
    chatChannel = item;
  })
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', function () {
  // On macOS it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', function () {
  // On macOS it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) {
    createWindow()

  }
})

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and require them here.

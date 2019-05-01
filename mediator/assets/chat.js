const electron = require('electron');
const {ipcRenderer} = require('electron');
const Swal = require('sweetalert2');
const http = require('http');
const axios = require('axios');

let chatChannel;

$.blockUI();

ipcRenderer.on('chat_channel_final', (e, item) => {
    chatChannel = item;
    console.log("Chat channel is: ", chatChannel);
    if (chatChannel == undefined) {
        Swal("Error: chatChannel is undefined => could not unlock UI. Try to restart the application");
    } else {
        $.unblockUI();
    }
    setInterval( () => {
        axios.post('http://157.230.29.114/get_chat_messages', {id:chatChannel.id,passw:chatChannel.password,type:chatChannel.type}).then((res) => {
            console.log(res.data.messages);
            console.log(res.data);
            console.log(res);
            var msg;
            for (msg of res.data.messages) {
                console.log("calling newMessageReceived with " + msg);
                newMessageReceived(msg);
            }
            
            console.log("-----------------");
        });
    }, 1000);
});

$(".messages").animate({ scrollTop: $(document).height() }, "fast");

$("#profile-img").click(function() {
    $("#status-options").toggleClass("active");
});

$(".expand-button").click(function() {
    $("#profile").toggleClass("expanded");
    $("#contacts").toggleClass("expanded");
});

$("#status-options ul li").click(function() {
    $("#profile-img").removeClass();
    $("#status-online").removeClass("active");
    $("#status-away").removeClass("active");
    $("#status-busy").removeClass("active");
    $("#status-offline").removeClass("active");
    $(this).addClass("active");
    
    if($("#status-online").hasClass("active")) {
    $("#profile-img").addClass("online");
    } else if ($("#status-away").hasClass("active")) {
    $("#profile-img").addClass("away");
    } else if ($("#status-busy").hasClass("active")) {
    $("#profile-img").addClass("busy");
    } else if ($("#status-offline").hasClass("active")) {
    $("#profile-img").addClass("offline");
    } else {
    $("#profile-img").removeClass();
    };
    
    $("#status-options").removeClass("active");
});

function newMessageReceived(msg) {
    $('<li class="replies"><img src="assets/images/avatar.bmp" alt="" /><p>' + msg + '</p></li>').appendTo($('.messages ul'));
    $('.message-input input').val(null);
    $('.contact.active .preview').html('<span>Him: </span>' + msg);
    $(".messages").animate({ scrollTop: $(document).height()+100000000 }, "fast");
}

function newMessage() {
    message = $(".message-input input").val();
    if($.trim(message) == '') {
    return false;
    }

    if (chatChannel) {
        axios.post('http://157.230.29.114/send_chat_messages', {id:chatChannel.id,passw:chatChannel.password,type:chatChannel.type,message:message}).then((res) => {
            console.log(`statusCode: ${res.statusCode}`);
            console.log(res);
        });
    } else {
        Swal("You can't send messages: chatChannel is undefined");
        return;
    }

    $('<li class="sent"><img src="assets/images/avatar.bmp" alt="" /><p>' + message + '</p></li>').appendTo($('.messages ul'));
    $('.message-input input').val(null);
    $('.contact.active .preview').html('<span>You: </span>' + message);
    $(".messages").animate({ scrollTop: $(document).height()+100000000 }, "fast");
    
};

$('.submit').click(function() {
    newMessage();
});

$(window).on('keydown', function(e) {
    if (e.which == 13) {
    newMessage();
    return false;
    }
});
//# sourceURL=pen.js
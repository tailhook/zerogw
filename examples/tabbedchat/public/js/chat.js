$(function() {
    var websock = new Connection("/ws");
    var handlers = {};
    websock.onopen = function() {
        $("#nobuttons").hide();
        $("#login, #register").show();
    }
    websock.onmessage = function(ev) {
        if(window.console) console.log("GOT", ev.data);
        var json = JSON.parse(ev.data)
        var meth = json.shift();
        var fun = handlers[meth];
        if(!fun) {
            alert("Unexpected data: " + meth)
        } else {
            fun.apply(null, json);
        }
    }
    function call() {
        if(window.console) console.log("CALLING", arguments);
        websock.send(JSON.stringify(Array.prototype.slice.call(arguments, 0)));
    }

    $("#preloader").hide();
    $("#tabs").tabs().tabs('disable', 1).tabs('disable', 2).show();
    $('button').button();
    $("#login, #register").hide();
    $("#login").click(function(ev) {
        ev.preventDefault();
        call('auth.login', {
            'login': $("#a_login").val(),
            'password': $("#a_password").val()
            })
    });
    $("#c_password1, #c_password2").change(function() {
        var v1 = $("#c_password1").val();
        var v2 = $("#c_password2").val();
        $("#c_ok").button(v1.length && v1 == v2 ? "enable" : "disable");
    });
    $("#register").click(function(ev) {
        ev.preventDefault();
        $("#c_password1").val($("#a_password").val());
        $("#confirm").dialog({
            'title': "Confirm password",
            'modal': true
            });
    });
    $("#c_ok").button({ "disabled": true }).click(function(ev) {
        ev.preventDefault();
        call('auth.register', {
            'login': $("#a_login").val(),
            'password': $("#c_password1").val()
            });
    });
    $("#n_add").click(function(ev) {
        call('chat.join', $("#n_channel").val());
    })

    handlers['auth.ok'] = handlers['auth.registered'] = function(info) {
        $("#tabs-register").hide();
        $("#tabs").tabs('enable', 1).tabs('enable', 2).tabs('remove', 0);
        $("#my_nickname").text(info.name);
        $("#my_mood").text(info.mood);
        if(info.rooms) {
            call('chat.join', info.rooms);
        } else if(info.bookmarks) {
            call('chat.join', info.bookmarks);
        } else {
            call('chat.join', 'kittens');
        }
    }
    handlers['chat.room'] = function(info) {
        var idx = $("#tabs").tabs("length");
        var ident = 'room_'+info.ident;
        $("#tabs").append($('<div class="room">').attr("id", ident))
            .tabs("add", '#'+ident, info.name, idx-2)
            .tabs("select", idx-2);
    }
});

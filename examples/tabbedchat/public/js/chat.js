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

    function userclick() {
        var ib = $(".inputbox input", $(this).parents('.room'));
        var v = ib.val();
        var u = $(this).data('username');
        if(v.length && v.trim().match(/\w$/))
            v += ', ' + u + ', ';
        else
            v += u + ', ';
        ib.val(v);
    }
    function mkmessage(msg) {
        if(msg.kind == 'join') {
            msg.text = "... joined room";
        }
        var res = $('<div class="msg">')
            .text(msg.text)
            .prepend($('<span class="user">')
                .text(msg.author)
                .data('username', msg.author));
        return res;
    }

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
        var div = $('<div class="room">'
            +'<div class="inputbox">'
            +'<input type="text" class="ui-corner-all">'
            +'<button class="send">Send</button>'
            +'</div>').attr("id", ident);
        $('button', div).button();
        var body = $('<div class="body ui-corner-all">');
        $.each(info.history, function(i, txt) {
            body.append(mkmessage(txt));
        });
        div.append(body);
        var userlist = $('<div class="userlist ui-corner-all">');
        $.each(info.users, function(i, user) {
            userlist.append($('<div class="user ui-corner-all">')
                .text(user.name).data('username', user.name)
                .append($('<span class="mood">').text(user.mood)));
        });
        div.append(userlist);
        $('.user', div).click(userclick);
        $('button.send', div).click(function() {
            call('chat.message', info.ident, $(".inputbox input", div).val());
        });
        $("#tabs").append(div)
            .tabs("add", '#'+ident, info.name, idx-2)
            .tabs("select", idx-2);
    }
    handlers['chat.message'] = function(room_id, data) {
        var msg = mkmessage(data)
        msg.find('.user').click(userclick);
        $('#room_'+room_id+' div.body').append(msg)

    }
});

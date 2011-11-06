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
        call('chat.join_by_name', $("#n_channel").val());
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
        } else if(msg.kidn == 'left') {
            msg.text = "... left room";
        }
        var res = $('<div class="msg">')
            .text(msg.text || '')
            .prepend($('<span class="user">')
                .text(msg.author)
                .data('username', msg.author));
        return res;
    }
    function mkuser(user, room_id) {
        return $('<div class="user ui-corner-all">')
                .attr('id', 'user_'+room_id+'_'+user.ident)
                .text(user.name).data('username', user.name)
                .append($('<span class="mood">').text(user.mood))
    }

    handlers['auth.ok'] = handlers['auth.registered'] = function(info) {
        $("#confirm").dialog("close");
        $("#tabs-register").hide();
        $("#tabs").tabs('enable', 1).tabs('enable', 2).tabs('remove', 0);
        $("#my_nickname").text(info.name);
        $("#my_mood").text(info.mood);
        document.cookie = 'session_id=' + info.session_id;
        if(info.rooms && info.rooms.length) {
            call('chat.join_by_ids', info.rooms);
        } else if(info.bookmarks && info.bookmarks.length) {
            call('chat.join_by_ids', info.bookmarks);
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
            userlist.append(mkuser(user, info.ident));
        });
        div.append(userlist);
        $('.user', div).click(userclick);
        function sendmessage() {
            var val = $(".inputbox input", div).val();
            call('chat.message', info.ident, val);
            $(".inputbox input", div).val('');
        }
        $('button.send', div).click(sendmessage);
        $(".inputbox input", div).keydown(function(ev) {
            if(ev.which == 13) {
                sendmessage();
                ev.preventDefault();
            }
        })
        $("#tabs").append(div)
            .tabs("add", '#'+ident, info.name, idx-2)
            .tabs("select", idx-2);
    }
    handlers['chat.message'] = function(room_id, data) {
        var msg = mkmessage(data)
        msg.find('.user').click(userclick);
        $('#room_'+room_id+' div.body').append(msg);
    }
    handlers['chat.joined'] = function(room_id, user) {
        var uel = mkuser(user, room_id);
        uel.find('.user').click(userclick);
        var div = $('#room_'+room_id+' div.userlist');
        if(!$('#user_'+room_id+'_'+user.ident).length) {
            div.append(uel);
            handlers['chat.message'](room_id, {
                'kind': 'join', 'author': user.name, 'uid': user.ident });
        }
    }
    handlers['chat.left'] = function(room_id, user) {
        $('#user_'+room_id+'_'+user.ident).remove();
    }
});

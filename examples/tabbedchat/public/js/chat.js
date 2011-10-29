$(function() {
    var websock = new Connection("/ws");
    websock.onopen = function() {
        $("#nobuttons").hide();
        $("#login, #register").show();
    }
    websock.onmessage = function(ev) {
        if(window.console) console.log("GOT", ev);
    }
    function call() {
        if(window.console) console.log("CALLING", arguments);
        websock.send(JSON.stringify(Array.prototype.slice.call(arguments, 0)));
    }

    $("#preloader").hide();
    $("#tabs").tabs().show();
    $("#login, #register").button().hide();
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
});

var app = {
    websocket: null,
    url: 'http://192.168.1.3',
    onmessage: function(event) {
        console.log(event);
        var time = 0;
        var re = /We lost our friend :\(/g;
        var found = event.data.match(re);

        // console.log('found', found);
        if (!found) {
            if ( 'We lost our friend :(')
            var json = JSON.parse(event.data);
            if (json) {
                console.log('ws',json);
                console.log('ws event.data',event.data);

                app.updateButtons(json);
            }
        }
    },
    request: function(url) {
        jQuery.ajax({
            type: "POST",
            url: app.url + url,
            dataType: 'json',
            success: function(data) {
                setTimeout(app.getStatus, 300);
            }
        });
    },
    updateButtons: function(data) {
        var values = [
            {name: '#light3', 'value': data.light3},
            {name: '#light2', 'value': data.light2},
            {name: '#light1', 'value': data.light1},
            {name: '#light0', 'value': data.light0},
        ];

        values.forEach(function(item) {
            var el = jQuery(item.name);
            console.log('test0', item);
            if (item.value) {
                el.removeClass('btn-primary');
                el.addClass('btn-warning');
            } else {
                el.addClass('btn-primary');
                el.removeClass('btn-warning');
            }
        })
    },
    getStatus: function() {
        jQuery.ajax({
            type: "POST",
            url: app.url + '/status',
            dataType: 'json',
            success: function(data) {
                app.updateButtons(data);
                console.log('/status data', data);
            }
        });
    },
    updateStateCallback: function(addr, value) {
            var state = ("000000000" + value.toString(2)).substr(-8).split("");
            state.forEach(function(state, pin) {
                var btn = jQuery("#" + app.buttonId('io', addr, 7 - pin))
                if (state == '1') {
                    btn.removeClass('btn-primary');
                    btn.addClass('btn-success');
                } else {
                    btn.addClass('btn-primary');
                    btn.removeClass('btn-success');
                }
            })
    },
    sendOn: function(pin) {
        app.request("/on?pin=" + pin)
    },
    sendOff: function(pin) {
        app.request("/off?pin=" + pin)
    },
    sendToggle: function(pin) {
        app.request("/toggle?pin=" + pin)
    },
    updateState: function() {
        jQuery.each(app.values, app.updateStateCallback)
    },
    init: function() {
        // for amazon hosting
        app.websocket = new WebSocket("ws://192.168.1.3/ws");

        // if (location.host == '') {
        //     app.websocket = new WebSocket("ws://192.168.1.3/ws");
        // } else {
        //     app.websocket = new WebSocket("ws://" + location.host + "/ws");
        // }
        app.websocket.onmessage = app.onmessage;
        app.attachButtonHandler();
        app.getStatus();
    },
    attachButtonHandler: function(btn) {
        console.log(jQuery('.toggle-button'));

        jQuery('.toggle-button').each(function(index, button) {
            jEl = jQuery(button);

            jEl.click(function() {
                jEl = jQuery(button);
                console.log('ololo', jEl.attr('data-pin'))
                app.sendToggle(jEl.attr('data-pin'))
            });

        })
    }
};

window.addEventListener("load", function() {
    app.init();
}, false);
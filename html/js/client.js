$(window).load(function() {

    // turn off context menu
    document.oncontextmenu = function(event) {
        if (event.preventDefault) {
            event.preventDefault();
        }
        if (event.stopPropagation) {
            event.stopPropagation();
        }
        event.cancelBubble = true;
        return false;
    }

    // create socket connection
    var ws = new WebSocket("ws://" + location.host + "/websocket");
    ws.onopen = (event) => {
        if(!$("#warning-message").is(":visible")) {
            $("#wrapper").show();
            $("#disconnect-message").hide();
        }
    };
    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        // expect only one message type

        var gamePadId = msg.player_no;
        $("#padText").html("<h1>Nr " + gamePadId + "</h1>");

        $(".btn")
            .off("touchstart touchend")
            .on("touchstart", function(event) {
                const msg = {
                    type: 0x01,
                    code: parseInt($(this).data("code"), 16),
                    value: 1
                };
                ws.send(JSON.stringify(msg));
                $(this).addClass("active");
            })
            .on("touchend", function(event) {
                const msg = {
                    type: 0x01,
                    code: parseInt($(this).data("code"), 16),
                    value: 0
                };
                ws.send(JSON.stringify(msg));
                $(this).removeClass("active");
            });
    };
    ws.onclose = (event) => {
        if(!$("#warning-message").is(":visible")) {
            $("#wrapper").hide();
            $("#disconnect-message").show();
        }
    };
    ws.onerror = (event) => {
        if(!$("#warning-message").is(":visible")) {
            $("#wrapper").hide();
            $("#disconnect-message").show();
        }
    };

    sendEvent = function(type, code, value) {
        let msg = {
            type: type,
            code: code,
            value: value
        }; 
        ws.send(JSON.stringify(msg));
    };

    convertDegreeToEvent = function(degree) {
        if (degree > 295 && degree < 335) {
            return 'right:down';
        } else if (degree >= 245 && degree <= 295) {
            return 'down';
        } else if (degree > 205 && degree < 245) {
            return 'left:down';
        } else if (degree >= 155 && degree <= 205) {
            return 'left';
        } else if (degree > 115 && degree < 155) {
            return 'left:up';
        } else if (degree >= 65 && degree <= 115) {
            return 'up';
        } else if (degree > 25 && degree < 65) {
            return 'right:up';
        } else if (degree <= 25 || degree >= 335) {
            return 'right';
        }
    };

    sendEventToServer = function(event) {
        console.log(event);
        switch (event) {
            case "left":
                sendEvent(0x03, 0x00, 0);
                sendEvent(0x03, 0x01, 127);
                break;
            case "left:up":
                sendEvent(0x03, 0x00, 0);
                sendEvent(0x03, 0x01, 0);
                break;
            case "left:down":
                sendEvent(0x03, 0x00, 0);
                sendEvent(0x03, 0x01, 255);
                break;
            case "right":
                sendEvent(0x03, 0x00, 255);
                sendEvent(0x03, 0x01, 127);
                break;
            case "right:up":
                sendEvent(0x03, 0x00, 255);
                sendEvent(0x03, 0x01, 0);
                break;
            case "right:down":
                sendEvent(0x03, 0x00, 255);
                sendEvent(0x03, 0x01, 255);
                break;
            case "up":
                sendEvent(0x03, 0x00, 127);
                sendEvent(0x03, 0x01, 0);
                break;
            case "down":
                sendEvent(0x03, 0x00, 127);
                sendEvent(0x03, 0x01, 255);
                break;
            default:
                sendEvent(0x03, 0x00, 127);
                sendEvent(0x03, 0x01, 127);
        }
    };

    var prevEvent;
    var prevAxis = {x: 127, y: 127};

    handle_input = function(vector) {
        //console.log(vector)
        var ev_x = vector.x;
        ev_x = ev_x >= 0.5 ? 255 : (ev_x <= -0.5 ? 0 : 127);
        var ev_y = vector.y;
        ev_y = ev_y <= -0.5 ? 255 : (ev_y >= 0.5 ? 0 : 127);
        if(ev_x != prevAxis.x)
            sendEvent(0x03, 0x00, ev_x);
        if(ev_y != prevAxis.y)
            sendEvent(0x03, 0x01, ev_y);
        prevAxis = {x: ev_x, y: ev_y};
    };

    // Create Joystick
    nipplejs.create({
            zone: document.querySelector('.joystick'),
            mode: 'static',
            color: 'white',
            position: {
                left: '50%',
                top: '50%'
            }
        })
        // start end
        .on('end', function(evt, data) {
            // set joystick to default position
            //sendEventToServer('end');
            //prevEvent = evt.type;
            handle_input({x: 0, y: 0});
            // dir:up plain:up dir:left plain:left dir:down plain:down dir:right plain:right || move
        }).on('move', function(evt, data) {
            /*
            var event = convertDegreeToEvent(data.angle.degree);
            if (event !== prevEvent) {
                sendEventToServer(event);
                prevEvent = event;
            }
            */
            handle_input(data.vector);
        })
        .on('pressure', function(evt, data) {
            console.log('pressure');
        });

    // Reload page when gamepad is disconnected
    $("#disconnect-message").click(function() {
        location.reload();
    });

});

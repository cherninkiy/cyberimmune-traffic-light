from bottle import run, get, post, request, route, response
from json import dumps


@get('/test')
def process_test():
    return '{"result": "test passed"}'


@route('/mode/<tl_id>', method = 'GET')
def process_test(tl_id):
    global tl_mode
    print(f'Requested mode for TL with ID {tl_id}\n{tl_mode}')
    response.content_type = 'application/json'
    return dumps(tl_mode)


@post('/manual')
def process_kos():
    data = request.json

    print(f'Manual mode request:\n{data}\n')

    if not 'mode' in data:
        print(f'Bad request: mode not set\n')
        result ={"result": "error", "error": "mode not set"}
        return dumps(result)
    
    if data['mode'] not in ['unregulated', 'regulated', 'manual']:
        print(f'Bad request: invalid mode="{data["mode"]}"\n')
        return f'Bad request: invalid mode="{data["mode"]}\n'

    if not 'direction_1' in data:
        print(f'Bad request: direction_1 not set\n')
        result ={"result": "error", "error": "direction_1 not set"}
        return dumps(result)

    if not 'direction_2' in data:
        print(f'Bad request: direction_2 not set\n')
        result ={"result": "error", "error": "direction_2 not set"}
        return dumps(result)

    global tl_mode
    tl_mode = data

    result ={"result": "success", "event": "mode changed"}
    return dumps(result)


@post('/diagnostics')
def process_kos():
    tl_data = request.json
    print(f'Received diagnostics:\n{tl_data}\n')
    result ={"result": "success", "event": "diagnostics received"}
    return dumps(result)


tl_mode = {
    # 'unregulated' | 'regulated' | 'manual'
    'mode': 'regulated',
    # if mode == 'regulated' then green light duration
    # if mode == 'manual' then color
    'direction_1': 30,
    # if mode == 'regulated' then green light duration
    # if mode == 'manual' then color
    'direction_2': 30
}

run(host='0.0.0.0', port=8081, debug=True)

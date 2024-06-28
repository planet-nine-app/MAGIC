### Overview

This is a very simple server that provides for an incoming spell payload via a REST api, forwards it to the [Continuebee] resolver, and emits that result via websocket as well as sending the result back the REST request path.
It's purpose is to connect things that want to listen for spell results, but not necessarily contribute to spell payloads themselves (this makes it easier to integrate with multiple MAGIC systems for things like games).

### API

<details>
 <summary><code>POST</code> <code><b>/spell</b></code> <code>POST a spell to resolve via Continuebee</code></summary>

##### Parameters

> | name         |  required     | data type               | description                                                           |
> |--------------|-----------|-------------------------|-----------------------------------------------------------------------|
> | spell        |  true     | object (see [MAGIC][M] for spell payload details)      | the signature from sessionless for the message  |


##### Responses

> | http code     | content-type                      | response                                                            |
> |---------------|-----------------------------------|---------------------------------------------------------------------|
> | `200`         | `application/json`                | `Spell response (see [MAGIC][M] for spell response`   |
> | `403`         | `application/json`                | `{"code":"403","message":"Spell fizzled"}`                            |

##### Example cURL

> ```javascript
>  curl -X POST -H "Content-Type: application/json" -d '<put spell here>' https:///spell
> ```

</details>

### Websocket

This server supports both standard websockets on port 4001, and socketio sockets on port 4002.
For now it only sends one event: 'magic', which your listener can respond to however they want.

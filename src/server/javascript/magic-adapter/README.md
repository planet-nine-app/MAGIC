# MAGIC gateway

This is the https magical gateway adapter for javascript server frameworks like express.

### Usage

```javascript
import express from 'express'; // for example
import gateway from 'magic-gateway-js';
import fount from 'fount-js'; // this and sessionless aren't strictly needed, but they are part of the same ecosystem
import sessionless from 'sessionless-node';

const saveKeys = (keys) => { /* save your keys here */ };
const getKeys = () => { /* return your keys here */ };

const fountUser = await fount.createUser(saveKeys, getKeys); // again, Fount isn't necessary, but you will need a user 
                                                             // registered with a MAGIC resolver
await sessionless.generateKeys(() => {}, getKeys); // you have to prime sessionless with how to getKeys.

const spellbook = getSpellbook(); // spellbooks are part of the MAGIC protocol, and you'll need one for your gateway.
                                  // the bdo-js package provides a method for this if you want to use BDO
const myStopName = 'cool-server'; // this string needs to map to a stopName in a spell in the spellbook for 
                                  // you to get a spell cast your way.

const extraForGateway = (spellName) => {
  // return optional extra stuff for the gateway here
};

const onSuccess = (req, res, result) => {
  // do something cool here. req and res are the standard request and response objects in the framework you're using
  result.myStopName = {};
  result.myStopName.says = 'yo'; // you can add to the result object you send back
  res.send(result); // the result needs to make it back to the caster for the spell to complete
};

const app = express(); // standard stuff here, make the app
app.use(express.json());

gateway.expressApp(app, fountUser, spellbook, myStopName, sessionless, extraForGateway, onSuccess);

app.listen(3000);
```

Woof!
I know that's a lot. 
I've tried to make it as simple and straight forward as possible, but MAGIC always comes with a cost, and the cost in this instance is a lack of a one-line solution (that would of course be possible if someone were to take all this and wrap it up in a single type of MAGIC, and if you do that, please let me know about it!).

So here's what's going on.
Spells have two plus n participants: the caster who initiates the spell, the resolver who resolves the spell, and n gateways who act in between the caster and resolver.
This lib sets you up to be a gateway (to learn more about these roles please see the [MAGIC repo][magic]).[^1]

Each participant needs to be registered with the resolver, so that's the first step here with creating the `fountUser`.

Each spell follows a path through transports and gateways, and those paths are defined in the `spellbook`. 
Destinations on the path are represented by a (name, route) tuple, and so your server needs a stopName that maps to where it fits in the spell.

In addition to forwarding the spell, and returning the response, gateways can do three things.
They can attach extra information to the spell object that gets forwarded, and modify the result object that gets sent back, and they can perform some action based on success or failure. 
That last thing is what makes this magical since this server could be some boring cloud whatever, or it could be a raspberry pi wired up to a discoball and strobe light and jukebox that plays September by Earth, Wind, and Fire when you snap your fingers.

But the good news is that once you've assembled all of the above, then all you need to do is call one method, and you're good to go :).


[magic]: https://www.github.com/planet-nine-app/magic
[^1]: The transport matters too. This lib is for https. In the future it may include other server transports like websockets and/or udp.

import { Application, Router } from "https://deno.land/x/oak/mod.ts";
import { oakCors } from "https://deno.land/x/cors/mod.ts";
import sessionless from "npm:sessionless-node@^0.10.1";
import { getUser, saveUser } from "./src/persistence/user.ts";

const register = async (context): object => {
  const request = context.request;
  const payload = await request.body.json();
  const signature = payload.signature;

  const message = JSON.stringify({
    timestamp: payload.timestamp,
    pubKey: payload.pubKey
  });

console.log(signature);

  if(!signature || !sessionless.verifySignature(signature, message, payload.pubKey)) {
console.log(signature);
console.log(message);
console.log(payload.pubKey);
console.log(payload);
    context.response.body = {
      status: 403, 
      error: 'Auth error'
    };
    return;
  }

  const uuid = sessionless.generateUUID();
  await saveUser(uuid, payload.pubKey, payload.hash);

  context.response.body = { uuid };
};

const checkHash = async (context): object => {
  const request = context.request;
  const url = new URL(request.url);
  const params = url.searchParams;
  const pathname = url.pathname;
  const message = JSON.stringify({
    timestamp: params.get('timestamp'),
    hash: params.get('hash')
  });
  const signature = params.get('signature');

  const uuid = pathname.split('/')[2];

  const user = await getUser(uuid);

  if(!signature || !sessionless.verifySignature(signature, message, user.pubKey)) {
    context.response.body = {
      status: 403,
      error: 'Auth error'
    }; 
    return;
  }

  if(user.hash === params.get('hash')) {
    context.response.body = { userUUID: uuid };
    return;
  } 
  context.response.body = {
    status: 406,
    error: 'Not acceptable'
  };
};

const saveHash = async (context): object => {
  const request = context.request;
  const payload = await request.body.json();
  const signature = payload.signature;
  const pathname = new URL(request.url).pathname;

  const message = JSON.stringify({
    timestamp: payload.timestamp,
    oldHash: payload.oldHash,
    hash: payload.hash
  });

  const uuid = pathname.split('/')[2];
  const user = await getUser(uuid);

  if(!signature || !sessionless.verifySignature(signature, message, user.pubKey)) {
    context.response.body = {
      status: 403,
      error: 'Auth error'
    };
    return;
  }

  await saveUser(user.uuid, user.pubKey, payload.hash);

  context.response.body = {
    userUUID: user.uuid
  };
};

const deleteUser = async (context): object => {
  context.response.body = {
    status: 501,
    error: 'Not implemented'
  };
};

const resolveSpell = async (context): object => {
  const request = context.request;
  const payload = await request.body.json();

console.log(payload);

  let resolved = true;

  for(let i = 0; i < payload.gateways.length; i++) {
    const gateway = payload.gateways[i];
    const user = await getUser(gateway.uuid);
    const signature = gateway.signature;

console.log('gateway', gateway);
console.log('gatewayUser', user);

    const message = JSON.stringify({
      timestamp: gateway.timestamp,
      uuid: gateway.uuid,
      minimumCost: gateway.minimumCost,
      ordinal: gateway.ordinal
    });
  
    if(!signature || !sessionless.verifySignature(signature, message, user.pubKey)) {
console.log(user.pubKey);
console.log("This message is unverified");
console.log(message);
console.log(signature);
console.log('verified???', sessionless.verifySignature(signature, message, user.pubKey));
      resolved = false;
    }

  }

  const user = await getUser(payload.casterUUID);
  const message = JSON.stringify({
    timestamp: payload.timestamp,
    spell: payload.spell,
    casterUUID: payload.casterUUID,
    totalCost: payload.totalCost,
    mp: payload.mp,
    ordinal: payload.ordinal,
  });

console.log('casterSignature', payload.casterSignature);

  if(!sessionless.verifySignature(payload.casterSignature, message, user ? user.pubKey : '03f60b3bf11552f5a0c7d6b52fcc415973d30b52ab1d74845f1b34ae8568a47b5f')) {
console.log('caster part is unverified');
console.log(message);
    resolved = false;
  }

  if(resolved) {
    context.response.body = {
      success: true
    };
    return;
  } 
  context.response.body = {
    status: 503,
    error: 'unauthorized'
  };
};

const router = new Router();
router.put("/user/create", register);
router.get("/user/:uuid", checkHash);
router.post("/user/:uuid/save-hash", saveHash);
router.delete("/user/:uuid", deleteUser);
router.post("/resolve/spell/:spellName", resolveSpell);

const app = new Application();
app.use(oakCors()); 
app.use(router.routes());

await app.listen({ port: 3000 });

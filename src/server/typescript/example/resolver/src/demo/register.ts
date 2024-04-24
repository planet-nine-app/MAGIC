import sessionless from "npm:sessionless-node@^0.10.0";
import chalk from "npm:chalk";
import { saveUser } from "../persistence";

export const register = async (request: Request): Response | Error => {
  const payload = await request.json();
  const signature = payload.signature;

  const message = JSON.stringify({
    pubKey: payload.pubKey,
    timestamp: payload.timestamp
  });

  if(!signature || !sessionless.verifySignature(signature, message, payload.pubKey)) {
    throw new Error('Auth error');
  }

  const uuid = sessionless.generateUUID();
  const user = {
    uuid,
    pubKey: payload.pubKey,
    associatedKeys: {},
    mp: 1000,
    lastSpell: new Date().getTime() + ''
  };
  await saveUser(user);

  console.log(chalk.green(`\n\nuser registered with uuid: ${uuid}`));

  return { uuid };
};

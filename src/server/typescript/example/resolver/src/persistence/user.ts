const kv = await Deno.openKv();

export type User = {
  uuid: string;
  pubKey: string;
  associatedKeys: object;
  mp: number;
  lastSpell: Date;
};

export const saveUser = async (user: User) => {
  await kv.set([user.uuid], {
    uuid: user.uuid,
    pubKey: user.pubKey,
    associatedKeys: user.associatedKeys,
    mp: user.mp,
    lastSpell: user.lastSpell
  });
  await kv.set([user.pubKey], user.uuid);
};

export const getUser = async (uuid) => {
  const user = await kv.get([uuid]);
  return user.value;
};


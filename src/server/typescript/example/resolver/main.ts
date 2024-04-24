import chalk from "npm:chalk";
import { register, resolve } from "./src";

const dispatch = async (request: Request): Response => {
console.log(request.url);
  const path = request.url.split('/').pop();
console.log(path);
  if(path === 'register') {
    return await register(request);
  } else if(path === 'resolve') {
    return await resolve(request);
  }
  throw new Error("Invalid path");
};

Deno.serve({port: 3002}, async (request: Request) => {
  const res = await dispatch(request);
  try {
  return new Response(JSON.stringify(res), {
    headers: {
      "content-type": "application/json; charset=utf-8"
    }
  });
  } catch(err) {
    return new Response(JSON.stringify(err), {
      status: 403,
      headers: {
        "content-type": "application/json; charset=utf-8"
      }
    });
  }
});

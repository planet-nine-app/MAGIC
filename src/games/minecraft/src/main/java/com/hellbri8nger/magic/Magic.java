package com.hellbri8nger.magic;

import io.socket.client.IO;
import io.socket.client.Socket;
import io.socket.emitter.Emitter;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.minecraft.server.MinecraftServer;
import net.minecraft.text.Text;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import java.net.URISyntaxException;

public class Magic implements ModInitializer {
	public static final String MOD_ID = "magic";
	public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

	@Override
	public void onInitialize() {
		// Runs as soon as minecraft is in a mod-load-ready state

		ServerLifecycleEvents.SERVER_STARTED.register(this::sendMessageToChat);

	}

	public void sendMessageToChat(MinecraftServer server){
		try {
			Socket socket = IO.socket("http://localhost:3001");

			socket.on(Socket.EVENT_CONNECT, new Emitter.Listener() {
				@Override
				public void call(Object... args) {
					LOGGER.info("Connected to the server");

					socket.on("magic", new Emitter.Listener() {
						@Override
						public void call(Object... args) {
							server.getPlayerManager().broadcast(Text.literal((String) args[0]), false);
						}
					});
				}
			});

			socket.connect();

		} catch (URISyntaxException e) {
			LOGGER.info("Unable to connect to the server");
			throw new RuntimeException(e);
		}
	}

}
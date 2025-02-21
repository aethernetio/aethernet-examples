package io.aether.examples;

import java.net.URI;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

import io.aether.cloud.client.AetherCloudClient;
import io.aether.cloud.client.ClientConfiguration;
import io.aether.StandardUUIDs;
import io.aether.utils.futures.AFuture;
import io.aether.utils.streams.Value;

public final class PointToPoint {

	public static void main(String[] args) {
		List<URI> cloudFactoryURI = new ArrayList<>();
		cloudFactoryURI.add(URI.create("tcp://registration.aethernet.io:9010"));
		ClientConfiguration clientConfig1 = new ClientConfiguration(StandardUUIDs.TEST_UID, cloudFactoryURI);
		ClientConfiguration clientConfig2 = new ClientConfiguration(StandardUUIDs.TEST_UID, cloudFactoryURI);
		AetherCloudClient client1 = new AetherCloudClient(clientConfig1);
		AetherCloudClient client2 = new AetherCloudClient(clientConfig2);
		if (!client1.startFuture.waitDoneSeconds(1000) || !client2.startFuture.waitDoneSeconds(1000)) {
			System.out.println("Error registering a client.");
		}
		System.out.println("Clients are registered.");
		client2.ping();
		AFuture checkReceiveMessage = new AFuture();
		var message = "Hello world!".getBytes();
		client2.onClientStream((m) -> {
			if (!Objects.equals(client1.getUid(), m.getConsumerUUID())) {
				System.out.println("Error: a message from unknown ID has been received.");
			}
			m.up().toConsumer(newMessage -> {
				if (!Arrays.equals(newMessage, message)) {
					System.out.println("Error: received message is corrupted.");
				} else {
					System.out.println("A message has been received.");
				}
				checkReceiveMessage.done();
			});
		});
		var chToc2 = client1.openStreamToClient(client2.getUid());
		var g = chToc2.toConsumer(v -> {
		});
		g.getFGateCast().inSide.send(Value.ofForce(message));
		System.out.println("A message has been sent.");

		if (!checkReceiveMessage.waitDoneSeconds(3000)) {
			System.out.println("Error: message time-out has been reached.");
		}
		client1.stop(5);
		client2.stop(5);
	}
}
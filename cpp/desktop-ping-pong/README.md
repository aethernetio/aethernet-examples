# Ping Pong for desktop

This is a simple ping pong example with aether client library. It registers two clients *Alice* and *Bob*. *Alice* sends "ping", *Bob* responds "pong".
The simplest example, but it covers a lot of things lib aether has: aether_app, object, actions, events, streams.

## Build
Simply use the script provided for different OSes:
```sh
build_and_run.sh
```
```sh
build_and_run.bat
```

## Deep dive
### The recipe to build
We are using **CMake** for now. Though some considered it industry standard other build systems also would be supported in the future.
Lib `aether` requires at least *c++17*, so require it on your project and look at the nex code snippet.

```cmake
# By default, distillation mode is off; turn it on for this example
if (NOT DEFINED AE_DISTILLATION)
  set(AE_DISTILLATION On)
endif()

# add user provided config wich will be included as regular .h file
# Add a user-provided config file, which will be included as a regular .h file
set(USER_CONFIG user_config.h)
# ${USER_CONFIG} must be an absolute path or a path to something listed in include directories
include_directories(${CMAKE_CURRENT_LIST_DIR})

# Add the Aether library dependency
add_subdirectory(aether-client-cpp/aether aether)
```

There are two modes `aether` works with: [distillation mode](https://aethernet.io/documentation#c++2) and production mode.
By default `aether` builds in production mode, but for example only we set `AE_DISTILLATION` option to `On` in cmake script directly.
In distillation mode `aether` configures all its inner objects in a default states and saves them to the file system.
Look at the `build/state` directory.
In production mode `aether` only loads objects with saved states from `./state`.
It allows not only save time and code to configure big objects, but, for more important, use saved state between application runs.

To configure `aether` library we use configuration header file. All configuration options with its default values listed in `aether/config.h`.
But user able provide his own through `USER_CONFIG` option.
It must be absolute path or path relative to something listed in include directories.

### Where it all begins
```cpp
int main() {
  auto aether_app = ae::AetherApp::Construct(ae::AetherAppConstructor{});

  std::unique_ptr<Alice> alice;
  std::unique_ptr<Bob> bob;

  auto client_register_action = ClientRegister{*aether_app};

  // Create a subscription to the Result event
  auto on_registered =
      client_register_action.SubscribeOnResult([&](auto const& action) {
        auto [client_alice, client_bob] = action.get_clients();
        alice = ae::make_unique<Alice>(*aether_app, std::move(client_alice),
                                       client_bob->uid());
        bob = ae::make_unique<Bob>(*aether_app, std::move(client_bob));
        // Save the current aether state
        aether_app->domain().SaveRoot(aether_app->aether());
      });

  // Subscription to the Error event
  auto on_register_failed = client_register_action.SubscribeOnError(
      [&](auto const&) { aether_app->Exit(1); });

  while (!aether_app->IsExited()) {
    auto next_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(next_time);
  }
  return aether_app->ExitCode();
}
```

Let's go through this!

First create `aether_app` - it's a single object in the aether lib to rule them all. It creates, initializes and provides access to the root [`aether`](https://aethernet.io/documentation#c++2) object,
and has a helper functions: `Update` and `WaitUntil` to easily integrate it into your update/event loop.

Define our main characters *Alice* and *Bob*.

Create an action to register clients in Aethernet - `client_register_action`.
[Action](https://aethernet.io/technology#action2) in aether is a concept to perform asynchronous operations.
Each action is inherited from `ae::Action<T>` and registered in [ActionProcessor](https://aethernet.io/technology#action2) infrastructure.
It has an `Update` method invoked every loop, where we can manage a state machine or check status of multithreaded tasks.
To inform about it's state three [events](https://aethernet.io/documentation#c++2) exists: `Result`, `Error`, `Stop` - names speaks for itself.

We subscribe to `Result` event and obtain clients for *Alice* and *Bob* from action to init our characters and also save current `aether` state.
On `Error` we close application with exit code 1 as soon as possible.

For this example clients for `Alice` and `Bob` registered every time application runs in distillation mode.
But in production mode clients from the saved state used.
Reconfigure cmake with `AE_DISTILLATION=Off` (just make in your build dir `cmake -DAE_DISTILLATION=Off .`) and rebuild it.
Next run will be a little faster without registration.

[Event subscription](https://aethernet.io/documentation#c++2) is a RAII object holds subscription to events and unsubscribes on destruction.

### Alice
```cpp
Alice::Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
             ae::Uid bobs_uid)
    : aether_{aether_app.aether()},
      client_alice_{std::move(client_alice)},
      p2pstream_{ae::ActionContext{*aether_->action_processor},
                 kSafeStreamConfig,
                 ae::make_unique<ae::P2pStream>(
                     ae::ActionContext{*aether_->action_processor},
                     client_alice_, bobs_uid, ae::StreamId{0})},
      interval_sender_{ae::ActionContext{*aether_->action_processor},
                       p2pstream_, std::chrono::milliseconds{5000}},
      interval_sender_subscription_{interval_sender_.SubscribeOnError(
          [&](auto const&) { aether_app.Exit(1); })} {}
```
*Alice* saves pointer to `aether` object, stores `client_alice`, and creates entities there all busyness magics happens.
She knows *Bob*'s [`uid`](https://aethernet.io/technology#registering-new-client0) and do not mind chatting with him.

Create `p2pstream_`. It's a [`P2pSafeStream`](https://aethernet.io/documentation#c++2), there *Safe* means it guarantees or makes all possible to deliver *Alice's* message to *Bob*.
Think about [streams](https://aethernet.io/documentation#c++2) like a tunnel through internet and Aethernet servers to another client.
It's full duplex, so you can scream yor messages to the tunnel and hear the answers simultaneously.

*Alice* uses `IntervalSender` - another action, to send her "pings" periodically.

```cpp
Alice::IntervalSender::IntervalSender(ae::ActionContext action_context,
                                      ae::ByteStream& stream,
                                      ae::Duration interval)
    : ae::Action<IntervalSender>{action_context},
      stream_{stream},
      interval_{interval},
      response_subscription_{stream.in().out_data_event().Subscribe(
          *this, ae::MethodPtr<&IntervalSender::ResponseReceived>{})} {}

ae::TimePoint Alice::IntervalSender::Update(ae::TimePoint current_time) {
  if (sent_time_ + interval_ <= current_time) {
    constexpr std::string_view ping_message = "ping";
    std::cout << "send \"ping\"" << '\n';
    auto send_action = stream_.in().Write(
        {std::begin(ping_message), std::end(ping_message)}, current_time);

    // notify about error
    send_subscriptions_.Push(send_action->SubscribeOnError([&](auto const&) {
      std::cerr << "ping send error" << '\n';
      ae::Action<IntervalSender>::Error(*this);
    }));

    sent_time_ = current_time;
  }

  return sent_time_ + interval_;
}
```
`IntervalSender` waits `interval_` time from last `sent_time_` and send new "ping" then. It also subscribed to response messages.

### Bob
```cpp
Bob::Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob)
    : aether_{aether_app.aether()},
      client_bob_{std::move(client_bob)},
      new_stream_receive_subscription_{
          client_bob_->client_connection()->new_stream_event().Subscribe(
              *this, ae::MethodPtr<&Bob::OnNewStream>{})} {}

void Bob::OnNewStream(ae::Uid destination_uid, ae::StreamId stream_id,
                      std::unique_ptr<ae::ByteStream> message_stream) {
  p2pstream_ = ae::make_unique<ae::P2pSafeStream>(
      ae::ActionContext{*aether_->action_processor}, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(
          ae::ActionContext{*aether_->action_processor}, client_bob_,
          destination_uid, stream_id, std::move(message_stream)));
  StreamCreated(*p2pstream_);
}

void Bob::StreamCreated(ae::ByteStream& stream) {
  message_receive_subscription_ =
      stream.in().out_data_event().Subscribe([&](auto const& data_buffer) {
        auto ping_message =
            std::string_view{reinterpret_cast<char const*>(data_buffer.data()),
                             data_buffer.size()};
        std::cout << "received " << std::quoted(ping_message) << '\n';

        constexpr std::string_view pong_message = "pong";
        stream.in().Write({std::begin(pong_message), std::end(pong_message)},
                          ae::Now());
      });
}
```

*Bob* forgot to ask *Alice*'s number (uid) and waits maybe she sends him a message.
He subscribed to `new_stream_event` of his connection to Aethernet object.
When *Alice* writes a first message to the stream, Aethernet connects it through the server directly to *Bob*.
He creates the same `P2pSafeStream` with the same properties as *Alice* and so he can parse, decrypt and receive the exact message as *Alice* has sent to him.
A happy *Bob*, so as not to force the girl wait, immediately sends his "pong" back.

## The end
I've not found a power to stop their chatting. So once you are tired of them, hit the `ctrl+c` or kill the process.

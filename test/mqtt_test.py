import paho.mqtt.client as mqtt
import json
import time

with open('test/config.json', 'r') as config_file:
    config = json.load(config_file)

CLIENT_ID = config["CLIENT_ID"]
BROKER_HOST = config["BROKER_HOST"]
BROKER_PORT = config["BROKER_PORT"]
TEST_TOPIC = config["TEST_TOPIC"]
WAIT_TIME_BEFORE_TEST = config["WAIT_TIME_BEFORE_TEST"]

class Test(object):
    def __init__(self, name, fun):
        self.name = name
        self.fun = fun
    
    def run(self):
        print(f"TEST CASE: {self.name}")
        wait_before_test()
        self.fun()

def wait_before_test():

    if WAIT_TIME_BEFORE_TEST == 0:
        return

    for i in range(WAIT_TIME_BEFORE_TEST, -1, -1):
        print(f"Test starting in {i}...", end='\r')
        time.sleep(1)

def on_message(client, userdata, message):
    print(f"Message: {message.payload.decode('utf-8')}")
    print(f"Topic: {message.topic}")
    print(f"QOS: {message.qos}")
    print(f"Retain flag: {message.retain}")

def test_subscribe():
    TEST_NAME = f"Subscribe to topic {TEST_TOPIC}."
    client.subscribe(TEST_TOPIC)
    client.on_message = on_message
    client.loop_start()
    time.sleep(5)
    client.loop_stop()
    client.unsubscribe(TEST_TOPIC)

def test_publish():
    TEST_NAME = f"Publish to topic {TEST_TOPIC}."
    client.publish(TEST_TOPIC, "75")

def run_tests():
    test_list.append(Test("SUBSCRIBE", test_subscribe))
    test_list.append(Test("PUBLISH", test_publish))

    for test in test_list:
        test.run()

test_list = list()
client = mqtt.Client(client_id=CLIENT_ID)
client.connect(host=BROKER_HOST, port=BROKER_PORT)
run_tests()
client.disconnect()
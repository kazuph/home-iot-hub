import paho.mqtt.client as mqtt
import json
import time

with open('config.json', 'r') as config_file:
    config = json.load(config_file)

CLIENT_ID = config["CLIENT_ID"]
BROKER_HOST = config["BROKER_HOST"]
BROKER_PORT = config["BROKER_PORT"]
TEST_TOPIC = config["TEST_TOPIC"]
WAIT_TIME_BEFORE_TEST = config["WAIT_TIME_BEFORE_TEST"]

test_list = list()
client = mqtt.Client(client_id=CLIENT_ID)
client.connect(host=BROKER_HOST, port=BROKER_PORT)

class Test(object):
    def __init__(self, name, fun):
        self.name = name
        self.fun = fun
    
    def run(self):
        print(f"TEST CASE: {self.name}")
        wait_before_test()
        self.fun()

def wait_before_test():
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
    time.sleep(30)
    client.loop_stop()

def main():
    test_list.append(Test("SUBSCRIBE", test_subscribe))

    for test in test_list:
        test.run()

main()
import unittest
from unittest import TestCase
from secrets import token_hex
from uuid import uuid4
from threading import Lock
import paho.mqtt.client as mqtt
import json
import time

class TestHomeIotHub(TestCase):

    PAYLOAD = None
    PAYLOAD_LOCK = Lock()

    def setUp(self):
        with open('test/config.json', 'r') as config_file:
            config = json.load(config_file)

        self.client_id = str(uuid4())
        self.host = config['BROKER_HOST']
        self.port = config['BROKER_PORT']
        self.client = mqtt.Client(client_id=self.client_id)
        self.client.connect(host=self.host, port=self.port)

    def tearDown(self):
        self.client.disconnect()

    def test_wifi_connect_disconnect(self):
        """
        TEST WIFI CONNECT DISCONNECT
        """
        TEST_ID = 0
        pass

    def test_mqtt_echo(self):
        """
        TEST MQTT ECHO
        Send 128 random tokens to client.
        """
        TEST_ID = 1
        topic_publish = '/hub_test/publish'
        topic_subscribe = '/hub_test/subscribe'

        test_string = token_hex(128)
        TestHomeIotHub.reset_payload()

        self.client.subscribe(topic_subscribe)
        self.client.on_message = TestHomeIotHub.on_message    
        self.client.loop_start()
        self.client.publish(topic_publish, test_string)
        time.sleep(1)
        self.client.loop_stop()
        self.client.unsubscribe(topic_subscribe)

        with TestHomeIotHub.PAYLOAD_LOCK:
            self.assertIsNotNone(TestHomeIotHub.PAYLOAD)
            self.assertEqual(test_string, str(TestHomeIotHub.PAYLOAD, 'utf-8'))

    def test_mqtt_echo_repeated(self):
        """
        TEST MQTT ECHO REPEATED
        Send 128 random tokens to client, repeat 10 times.
        """
        TEST_ID = 2
        topic_publish = '/hub_test/publish'
        topic_subscribe = '/hub_test/subscribe'
        reruns = 10

        for i in range(0, reruns):
            test_string = token_hex(128)
            TestHomeIotHub.reset_payload()

            self.client.subscribe(topic_subscribe)
            self.client.on_message = TestHomeIotHub.on_message    
            self.client.loop_start()
            self.client.publish(topic_publish, test_string)
            time.sleep(1)
            self.client.loop_stop()
            self.client.unsubscribe(topic_subscribe)

            with TestHomeIotHub.PAYLOAD_LOCK:
                self.assertIsNotNone(TestHomeIotHub.PAYLOAD)
                self.assertEqual(test_string, str(TestHomeIotHub.PAYLOAD, 'utf-8'))

    @staticmethod
    def on_message(client, userdata, message):
        with TestHomeIotHub.PAYLOAD_LOCK:
            TestHomeIotHub.PAYLOAD = message.payload

    @staticmethod
    def reset_payload():
        with TestHomeIotHub.PAYLOAD_LOCK:
            TestHomeIotHub.PAYLOAD = None

if __name__ == '__main__':
    unittest.main()
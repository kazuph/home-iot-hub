import unittest
import serial
from unittest import TestCase
from secrets import token_hex
from uuid import uuid4
from threading import Lock
import paho.mqtt.client as mqtt
import re
import json
import time

TEST_SUCCESS = 0
TEST_FAILURE = -1
TEST_SETUP_FAILURE = -2
TEST_TIMEOUT = -3

class TestHomeIotHub(TestCase):

    PAYLOAD = None
    PAYLOAD_LOCK = Lock()
    TEST_RESULT_WAIT_TIMEOUT = 5 # seconds
    TEST_RESULT_REGEX = r'TEST RESULT:\s(\d+)'
    TEST_RESULT_PATTERN = re.compile(TEST_RESULT_REGEX)

    def setUp(self):
        with open('test/config.json', 'r') as config_file:
            config = json.load(config_file)

        self.client_id = 'fd147369-ed28-456e-abf1-1eaa195f8d75'
        self.host = config['BROKER_HOST']
        self.port = config['BROKER_PORT']
        self.client = mqtt.Client(client_id=self.client_id)
        self.client.connect(host=self.host, port=self.port)
        self.serial = serial.Serial('COM7', 74880, timeout=1)

    def tearDown(self):
        self.serial.close()
        self.client.disconnect()

    def test_wifi_connect_disconnect(self):
        """
        TEST WIFI CONNECT DISCONNECT
        """
        TEST_ID = b'\x00'
        self.serial.write(TEST_ID)
        self.wait_for_test_result()

    def test_mqtt_echo(self):
        """
        TEST MQTT ECHO
        Send 128 random tokens to client.
        """
        TEST_ID = b'\x01'
        self.serial.write(TEST_ID)
        topic_publish = '/test/publish'
        topic_subscribe = '/test/subscribe'

        test_string = token_hex(128)
        TestHomeIotHub.reset_payload()

        self.client.subscribe(topic_subscribe)
        self.client.on_message = TestHomeIotHub.on_message
        self.client.publish(topic_publish, test_string)
        self.client.loop_start()

        time.sleep(5.0)

        with TestHomeIotHub.PAYLOAD_LOCK:
            self.assertIsNotNone(TestHomeIotHub.PAYLOAD)
            self.assertEqual(test_string, str(TestHomeIotHub.PAYLOAD[:len(test_string)], 'utf-8'))

        self.client.loop_stop()
        self.client.unsubscribe(topic_subscribe)
        self.wait_for_test_result()

    def test_mqtt_echo_repeated(self):
        """
        TEST MQTT ECHO REPEATED
        Send 128 random tokens to client, repeat 10 times.
        """
        TEST_ID = b'\x02'
        topic_publish = '/test/publish'
        topic_subscribe = '/test/subscribe'
        reruns = 10

        self.client.subscribe(topic_subscribe)
        self.client.on_message = TestHomeIotHub.on_message
        self.client.loop_start()

        self.serial.write(TEST_ID)

        for i in range(0, reruns):
            test_string = token_hex(128)
            TestHomeIotHub.reset_payload()
  
            self.client.publish(topic_publish, test_string)
            self.client.loop_start()

            time.sleep(5.0)

            with TestHomeIotHub.PAYLOAD_LOCK:
                self.assertIsNotNone(TestHomeIotHub.PAYLOAD)
                self.assertEqual(test_string, str(TestHomeIotHub.PAYLOAD[:len(test_string)], 'utf-8'))

        self.client.loop_stop()
        self.client.unsubscribe(topic_subscribe)
        self.wait_for_test_result()

    def test_mqtt_receive_json(self):
        """
        TEST MQTT RECEIVE JSON
        """
        TEST_ID = b'\x03'
        self.serial.write(TEST_ID)
        
        topic = '/test/json'
        TestHomeIotHub.reset_payload()
        
        self.client.subscribe(topic)
        self.client.on_message = TestHomeIotHub.on_message
        self.client.loop_start()

        time.sleep(5.0)

        with TestHomeIotHub.PAYLOAD_LOCK:
            self.assertIsNotNone(TestHomeIotHub.PAYLOAD)

        self.client.loop_stop()
        self.client.unsubscribe(topic)
        self.wait_for_test_result()

    def wait_for_test_result(self):
        while True:
            try:
                line = str(self.serial.readline(), 'utf-8')
            except UnicodeDecodeError:
                continue
            result = re.findall(TestHomeIotHub.TEST_RESULT_PATTERN, line)
            if bool(result) == True:
                result = int(result[0])
                self.assertEqual(TEST_SUCCESS, result)
                return

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
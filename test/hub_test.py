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

PAYLOAD = None
PAYLOAD_LOCK = Lock()

TEST_RESULT_REGEX = r'TEST RESULT:\s(\d+)'
TEST_RESULT_PATTERN = re.compile(TEST_RESULT_REGEX)

def reset_payload():
    with PAYLOAD_LOCK:
        PAYLOAD = None

def get_payload():
    with PAYLOAD_LOCK:
        return PAYLOAD

def on_mqtt_message(client, userdata, message):
    #with PAYLOAD_LOCK:
        #PAYLOAD = message.payload
    PAYLOAD = dict(json.loads(str(message.payload, 'utf-8')))
    print(PAYLOAD)

class TestHomeIotHub(TestCase):

    def setUp(self):
        with open('test/config.json', 'r') as config_file:
            config = json.load(config_file)

        self.client = mqtt.Client(client_id='fd147369-ed28-456e-abf1-1eaa195f8d75')
        self.client.connect(host=config['BROKER_HOST'], port=config['BROKER_PORT'])
        self.serial = serial.Serial(config['SERIAL_PORT'], config['SERIAL_BAUDRATE'], timeout=5.0)

        # Wait for device setup
        time.sleep(3.0)

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

        topic_publish = '/test/publish'
        topic_subscribe = '/test/subscribe'

        test_string = token_hex(4)
        reset_payload()

        self.serial.write(TEST_ID)
        self.client.subscribe(topic_subscribe)
        self.client.on_message = on_mqtt_message
        self.client.publish(topic_publish, test_string)
        self.client.loop_start()


        payload = get_payload()
        self.assertIsNotNone(payload, 'No data received.')
        self.assertEqual(test_string, str(payload[:len(test_string)], 'utf-8'), 'Received message does not match the send one.')

        self.client.loop_stop()
        self.client.unsubscribe(topic_subscribe)
        self.wait_for_test_result()

    def wait_for_test_result(self):
        while True:
            line = str(self.serial.readline(), 'ascii')

            if (bool(line) == True):
                print(line)

            result = re.findall(TEST_RESULT_PATTERN, line)
            if bool(result) == True:
                result = int(result[0])
                self.assertEqual(TEST_SUCCESS, result, f'Test failed with error code {result}.')
                return

if __name__ == '__main__':
    #unittest.main()
    client = mqtt.Client(client_id='fd147369-ed28-456e-abf1-1eaa195f8d75')
    client.connect(host='192.168.0.109', port=1883)
    client.subscribe('#')
    client.on_message = on_mqtt_message
    client.loop_start()
    time.sleep(360.0)
    client.loop_stop()
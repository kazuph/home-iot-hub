import unittest
from unittest import TestCase
from secrets import token_hex
from uuid import uuid4
import paho.mqtt.client as mqtt
import json
import time

class TestHomeIotHub(TestCase):

    PAYLOAD = None

    def setUp(self):
        with open('test/config.json', 'r') as config_file:
            config = json.load(config_file)

        self.client_id = str(uuid4())
        self.host = config['BROKER_HOST']
        self.port = config['BROKER_PORT']
        self.client = mqtt.Client(client_id=self.client_id)
        self.client.connect(host=self.host, port=self.port)

    def test_echo(self):
        """
        Hub should be subscribed to /hub_test/publish topic
        and as soon as it reads the message it publishes it 
        to '/hub_test/subscribe' topic.
        """

        topic_publish = '/hub_test/publish'
        topic_subscribe = '/hub_test/subscribe'
        test_string = str(token_hex(16))

        self.client.subscribe(topic_subscribe)
        self.client.on_message = TestHomeIotHub.on_message    
        self.client.loop_start()
        self.client.publish(topic_publish, test_string)
        time.sleep(5)
        self.client.loop_stop()
        self.client.unsubscribe(topic_subscribe)

        self.assertEqual(TestHomeIotHub.PAYLOAD, test_string)

    @staticmethod
    def on_message(client, userdata, message):
        TestHomeIotHub.PAYLOAD = message.payload.decode('utf-8')

if __name__ == '__main__':
    unittest.main()
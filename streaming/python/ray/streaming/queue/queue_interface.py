from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from enum import Enum
from abc import ABCMeta, abstractmethod


class QueueConfig:
    """
    queue config
    """
    # operator type
    OPERATOR_TYPE = "operator_type"

    # reliability level
    RELIABILITY_LEVEL = "reliability_level"


class OperatorType(Enum):
    """
    operator type
    """
    SOURCE = 1
    TRANSFORM = 2
    SINK = 3


class ReliabilityLevel(Enum):
    """
    reliability level
    """
    AT_LEAST_ONCE = 1
    EXACTLY_ONCE = 2
    EXACTLY_SAME = 3


class QueueItem:
    """
    queue item interface
    """

    __metaclass__ = ABCMeta

    def __init__(self):
        pass

    @abstractmethod
    def body(self):
        pass

    @abstractmethod
    def timestamp(self):
        pass


class QueueMessage(QueueItem):
    """
    queue message interface
    """

    __metaclass__ = ABCMeta

    @abstractmethod
    def queue_id(self):
        pass


class QueueLink:
    """
    queue link interface
    """

    __metaclass__ = ABCMeta

    @abstractmethod
    def set_ray_runtime(self, runtime):
        """
        Set ray runtime config
        :param runtime:  ray runtime config
        """
        pass

    @abstractmethod
    def set_configuration(self, conf):
        """
        Set queue configuration
        :param conf:  queue configuration
        """
        pass

    @abstractmethod
    def register_queue_consumer(self, input_queue_ids):
        """
        Get queue consumer of input queues
        :param input_queue_ids:  input queue ids
        :return:  queue consumer
        """
        pass

    @abstractmethod
    def register_queue_producer(self, output_queue_ids):
        """
        Get queue producer of output queue ids
        :param output_queue_ids:
        :return:  queue producer
        """
        pass


class QueueProducer:
    """
    queue producer interface
    """

    __metaclass__ = ABCMeta

    @abstractmethod
    def produce(self, queue_id, item):
        """
        Produce msg into the special queue
        :param queue_id:  the specified queue id
        :param item:  the message
        """
        pass

    @abstractmethod
    def stop(self):
        """
        stop produce to avoid blocking
        :return: None
        """
        pass

    @abstractmethod
    def close(self):
        """
        Close the queue producer to release resource
        """
        pass


class QueueConsumer:
    """
    queue consumer interface
    """

    __metaclass__ = ABCMeta

    @abstractmethod
    def pull(self, timeout_millis):
        pass

    @abstractmethod
    def stop(self):
        pass

    @abstractmethod
    def close(self):
        pass
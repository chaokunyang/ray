import importlib
from abc import ABC, abstractmethod

import cloudpickle
from ray.streaming.runtime import gateway_client


class Partition(ABC):
    """Interface of the partitioning strategy."""

    @abstractmethod
    def partition(self, record, num_partition: int):
        """Given a record and downstream partitions, determine which partition(s)
         should receive the record.

        Args:
            record: The record.
            num_partition: num of partitions
        Returns:
            IDs of the downstream partitions that should receive the record.
         """
        pass


class BroadcastPartition(Partition):
    """Broadcast the record to all downstream partitions."""

    def __init__(self):
        self.__partitions = []

    def partition(self, record, num_partition: int):
        if len(self.__partitions) != num_partition:
            self.__partitions = list(range(num_partition))
        return self.__partitions


class KeyPartition(Partition):
    """Partition the record by the key."""

    def __init__(self):
        self.__partitions = [-1]

    def partition(self, key_record, num_partition: int):
        self.__partitions[0] = abs(hash(key_record.key())) % num_partition
        return self.__partitions


class RoundRobinPartition(Partition):
    """Partition record to downstream tasks in a round-robin matter."""

    def __init__(self):
        self.__partitions = [-1]
        self.seq = 0

    def partition(self, key_record, num_partition: int):
        self.seq = (self.seq + 1) % num_partition
        self.__partitions[0] = self.seq
        return self.__partitions


class SimplePartition(Partition):
    def __init__(self, func):
        self.func = func

    def partition(self, record, num_partition: int):
        return self.func(record, num_partition)


def serialize(partition_func):
    return cloudpickle.dumps(partition_func)


def deserialize(partition_bytes):
    return cloudpickle.loads(partition_bytes)


def load_partition(descriptor_partition_bytes):
    partition_bytes, module_name, class_name, function_name = gateway_client.deserialize(
        descriptor_partition_bytes)
    if partition_bytes:
        return deserialize(partition_bytes)
    else:
        assert module_name
        mod = importlib.import_module(module_name)
        if class_name:
            assert function_name is None
            cls = getattr(mod, class_name)
            return cls()
        else:
            assert function_name
            func = getattr(mod, function_name)
            return SimplePartition(func)

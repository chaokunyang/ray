from abc import ABC, abstractmethod


class RuntimeContext(ABC):
    @abstractmethod
    def get_task_id(self):
        pass

    @abstractmethod
    def get_task_index(self):
        pass

    @abstractmethod
    def get_parallelism(self):
        pass


class RuntimeContextImpl(RuntimeContext):
    def __init__(self, execution_task, parallelism: int):
        self.task_id = execution_task.get_task_id()
        self.task_index = execution_task.get_task_index()
        self.parallelism = parallelism

    def get_task_id(self):
        return self.task_id

    def get_task_index(self):
        return self.task_index

    def get_parallelism(self):
        return self.parallelism

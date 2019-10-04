from typing import List
from pcp.pmda import PMDA, pmdaIndom, pmdaInstid


class UncachedIndom:
    """manage indom for a bpftrace map"""

    def __init__(self, pmda: PMDA, serial: int):
        """
        UncachedIndom constructor

        :param serial: indom serial, needs to be unique across PMDA
        """
        self.pmda = pmda
        self.indom_id: int = self.pmda.indom(serial)
        self.indom = pmdaIndom(self.indom_id, [])
        self.pmda.add_indom(self.indom)

        self.next_instance_id = 0
        self.instance_names = []
        self.instance_id_lookup = {}
        self.instance_name_lookup = {}

    def inst_id_lookup(self, instance_name: str) -> int:
        """get instance id by instance name"""
        inst_id = self.instance_id_lookup.get(instance_name)
        if inst_id is None:
            inst_id = self.next_instance_id
            self.next_instance_id += 1
            self.instance_id_lookup[instance_name] = inst_id
            self.instance_name_lookup[inst_id] = instance_name
        return inst_id

    def inst_name_lookup(self, instance_id: int) -> str:
        """get instance name by instance id"""
        return self.instance_name_lookup.get(instance_id)

    def update(self, instance_names: List[str], key_fn=None):
        """update indom with new instance names"""
        instance_names_sorted = sorted(instance_names, key=key_fn)
        # TODO
        if self.instance_names != instance_names_sorted:
            instances = [pmdaInstid(self.inst_id_lookup(name), name) for name in instance_names_sorted]
            self.indom.set_instances(self.indom_id, instances)
            self.instance_names = instance_names_sorted

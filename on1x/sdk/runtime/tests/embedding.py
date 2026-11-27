import copy
import ctypes
import sys
import unittest
from concurrent.futures import ThreadPoolExecutor
from on1x_rt import Error, Host


class EmbeddingTest(unittest.TestCase):
    def test_host(self):
        owner = Host(sys.argv[1])
        with owner as host:
            host.register_static(ctypes.CDLL(sys.argv[2]))
            host.instantiate()
            with self.assertRaisesRegex(Error, "already owns"):
                host.register_static(ctypes.CDLL(sys.argv[2]))
            self.assertEqual(host.call_i64("choose", 2**63-1, -2**63), 2**63-1)
            for name, args, status in [("missing", (), 6), ("choose", (1,), 12),
                                       ("fail", (), 13)]:
                with self.assertRaises(Error) as caught:
                    host.call_i64(name, *args)
                self.assertEqual(caught.exception.status, status)
            self.assertEqual(caught.exception.code, 4321)
            self.assertEqual(str(caught.exception), "provider failed")
            with self.assertRaises(OverflowError):
                host.call_i64("choose", 2**63, 1)
            with self.assertRaises(TypeError):
                copy.deepcopy(host)
            with ThreadPoolExecutor(1) as other, self.assertRaises(RuntimeError):
                other.submit(host.close).result()
        owner.close()
        with self.assertRaises(RuntimeError):
            owner.call_i64("choose", 1, 2)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])

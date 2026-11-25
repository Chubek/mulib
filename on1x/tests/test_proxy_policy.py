#!/usr/bin/env python3
"""Exercise proxy selection without consulting ambient settings or the network."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import on1x_proxy as policy


class ProxyPolicyTests(unittest.TestCase):
    def test_case_order_and_bypass(self):
        ambient = {'hTTps_pRoXy': 'http://mixed.invalid', 'HTTPS_PROXY': 'http://upper.invalid',
                   'https_proxy': 'http://lower.invalid', 'aNY_PrOxY': 'http://any.invalid',
                   'No_pRoXy': '127.0.0.1, .example.net', 'GIT_SSL_NO_VERIFY': 'true'}
        self.assertEqual(policy.git_environment({}, ambient),
                         {'https_proxy': 'http://lower.invalid', 'no_proxy': '127.0.0.1, .example.net'})
        self.assertEqual(policy.git_environment({}, {'hTTps_pRoXy': 'http://mixed.invalid'}),
                         {'https_proxy': 'http://mixed.invalid'})
        self.assertEqual(policy.git_environment({}, {'aNY_PrOxY': 'http://any.invalid'}),
                         {'https_proxy': 'http://any.invalid'})

    def test_project_precedence_and_no_plain_http(self):
        ambient = {'HTTPS_PROXY': 'http://ambient.invalid'}
        project = {'proxy-http': 'http://plain.invalid',
                   'proxy-https': 'http://specific.invalid', 'proxy-any': 'http://any.invalid'}
        self.assertEqual(policy.git_environment(project, ambient),
                         {'https_proxy': 'http://specific.invalid'})
        del project['proxy-https']
        self.assertEqual(policy.git_environment(project, ambient), {'https_proxy': 'http://any.invalid'})
        del project['proxy-any']
        self.assertEqual(policy.git_environment(project, {'HTTP_PROXY': 'http://ambient.invalid'}), {})
        self.assertEqual(policy.git_environment({}, {'ALL_PROXY': 'http://unlisted.invalid'}), {})
        self.assertEqual(policy.git_environment({'proxy-none': True}, ambient), {})

    def test_invalid_policy_does_not_reveal_value(self):
        secret = 'http://user:fixture-secret@proxy.invalid\n'
        with self.assertRaisesRegex(ValueError, 'proxy-https') as error:
            policy.manifest_url('proxy-https', secret)
        self.assertNotIn('fixture-secret', str(error.exception))
        with self.assertRaisesRegex(ValueError, 'HTTPS_PROXY') as error:
            policy.git_environment({}, {'HTTPS_PROXY': secret})
        self.assertNotIn('fixture-secret', str(error.exception))
        with self.assertRaisesRegex(ValueError, 'proxy-none cannot be combined'):
            policy.validate_manifest({'proxy-none': True, 'proxy-any': 'http://proxy.invalid'})


if __name__ == '__main__':
    unittest.main()

create table public.device_status (
  device_id text not null,
  last_seen timestamp with time zone not null default now(),
  wifi_rssi integer null,
  uptime_seconds bigint null,
  firmware_version text null,
  last_reboot_reason text null,
  device_timestamp bigint null,
  constraint device_status_pkey primary key (device_id)
) TABLESPACE pg_default;
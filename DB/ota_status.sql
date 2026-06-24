create table public.ota_status (
  id bigint generated always as identity not null,
  device_id text not null,
  from_version text null,
  to_version text null,
  phase text not null,
  error text null,
  attempt integer null,
  reported_at timestamp with time zone not null default now(),
  constraint ota_status_pkey primary key (id),
  constraint ota_status_phase_check check (
    (
      phase = any (
        array[
          'failed'::text,
          'applied'::text,
          'success'::text,
          'degraded'::text
        ]
      )
    )
  )
) TABLESPACE pg_default;

create index IF not exists ota_status_device_idx on public.ota_status using btree (device_id) TABLESPACE pg_default;

create index IF not exists ota_status_time_idx on public.ota_status using btree (reported_at desc) TABLESPACE pg_default;
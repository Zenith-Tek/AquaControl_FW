create table public.device_control (
  id integer not null,
  motor_state boolean not null default false,
  auto_control_enabled boolean not null default true,
  auto_on_enabled boolean not null default true,
  auto_off_enabled boolean not null default true,
  tank_size_cm bigint null,
  auto_on_level bigint null default '30'::bigint,
  auto_off_level bigint null default '85'::bigint,
  user_id uuid null,
  device_id text null,
  pair_with_sender_id text null,
  constraint device_control_pkey primary key (id),
  constraint device_control_user_id_fkey foreign KEY (user_id) references auth.users (id) on delete CASCADE
) TABLESPACE pg_default;